/*
    SPDX-FileCopyrightText: 2004 Duncan Mac-Vicar Prett <duncan@kde.org>
    SPDX-FileCopyrightText: 2004-2005 Olivier Goffart <ogoffart@kde.org>
    SPDX-FileCopyrightText: 2011 Niels Ole Salscheider
    <niels_ole@salscheider-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "latexrenderer.h"

#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <QDebug>

#include <KLocalizedString>

#include <QColor>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>
#include <QLibrary>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "gui/debug_ui.h"
#include "settings.h"

namespace GuiUtils
{
namespace
{
QString texInvocationLogPath()
{
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (logDir.isEmpty()) {
        logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (logDir.isEmpty()) {
        logDir = QDir::tempPath();
    }
    QDir().mkpath(logDir);
    return QDir(logDir).filePath(QStringLiteral("scholia-tex-debug.log"));
}

void logTexInvocation(const char *operation, const QString &backend, const QString &reason, const QStringList &details = QStringList())
{
    if (!OkularUiDebug().isDebugEnabled()) {
        return;
    }

    QStringList fields = {QStringLiteral("Invoking TeX; operation: %1").arg(QLatin1String(operation)), QStringLiteral("backend: %1").arg(backend), QStringLiteral("reason: %1").arg(reason)};
    for (const QString &detail : details) {
        fields << detail;
    }
    const QString message = fields.join(QStringLiteral("; "));
    qCDebug(OkularUiDebug).noquote() << message;

    QFile logFile(texInvocationLogPath());
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << message << '\n';
    }
}

#ifdef Q_OS_WIN
struct StemTeXConfig {
    const char *repo_root_utf8;
    const char *runtime_root_utf8;
    const char *texmf_root_utf8;
    const char *profile_root_utf8;
    const char *state_root_utf8;
    const char *renders_root_utf8;
    int request_timeout_ms;
    int xdvipdfmx_timeout_ms;
    int min_width_pt;
    int max_width_pt;
    int default_width_pt;
    int spare_worker_count;
    int auto_restart;
    int delete_intermediates;
    const char *worker_template_utf8;
};

struct StemTeXRenderResult {
    char *request_id_utf8;
    char *pdf_path_utf8;
    char *summary_json_utf8;
    int outcome_code;
    int issue_flags;
    char *outcome_message_utf8;
};

static constexpr int StemTeXRenderOutcomeOk = 0;

struct StemTeXEngineSnapshot {
    int status;
    int stage;
    int primary_ready;
    int spare_ready;
    int spare_target;
    int spare_rebuilding;
    int async_running;
    int async_pending;
    uint64_t running_job_id;
    uint64_t pending_job_id;
    int last_error;
};

struct StemTeXProfileInfo {
    QString name;
    QString path;
};

struct StemtexApi {
    using RenderCallback = void (*)(uint64_t, int, const StemTeXRenderResult *, int, const char *, void *);
    using Create = void *(*)(const StemTeXConfig *, int *, char **);
    using Render = int (*)(void *, const char *, int, StemTeXRenderResult *, int *, char **);
    using RenderAsync = int (*)(void *, const char *, int, uint64_t *, RenderCallback, void *, int *, char **);
    using EngineSnapshot = int (*)(void *, StemTeXEngineSnapshot *);
    using ProfileInfo = char *(*)(const char *, int *, char **);
    using ValidateConfig = int (*)(const StemTeXConfig *, int *, char **);
    using FreeResult = void (*)(StemTeXRenderResult *);
    using FreeString = void (*)(char *);
    using Destroy = void (*)(void *);

    QLibrary library;
    Create create = nullptr;
    Render render = nullptr;
    RenderAsync renderAsync = nullptr;
    EngineSnapshot engineSnapshot = nullptr;
    ProfileInfo profileInfo = nullptr;
    ValidateConfig validateConfig = nullptr;
    FreeResult freeResult = nullptr;
    FreeString freeString = nullptr;
    Destroy destroy = nullptr;

    explicit StemtexApi(const QString &dllPath)
        : library(dllPath)
    {
    }

    bool load(QString *error)
    {
        if (!library.load()) {
            if (error) {
                *error = library.errorString();
            }
            return false;
        }

        create = reinterpret_cast<Create>(library.resolve("stemtex_renderer_create"));
        render = reinterpret_cast<Render>(library.resolve("stemtex_renderer_render"));
        renderAsync = reinterpret_cast<RenderAsync>(library.resolve("stemtex_renderer_render_async"));
        engineSnapshot = reinterpret_cast<EngineSnapshot>(library.resolve("stemtex_renderer_engine_snapshot"));
        profileInfo = reinterpret_cast<ProfileInfo>(library.resolve("stemtex_renderer_profile_info_json"));
        validateConfig = reinterpret_cast<ValidateConfig>(library.resolve("stemtex_renderer_validate_config"));
        freeResult = reinterpret_cast<FreeResult>(library.resolve("stemtex_renderer_free_result"));
        freeString = reinterpret_cast<FreeString>(library.resolve("stemtex_renderer_free_string"));
        destroy = reinterpret_cast<Destroy>(library.resolve("stemtex_renderer_destroy"));
        const bool ok = create && render && renderAsync && engineSnapshot && profileInfo && validateConfig && freeResult && freeString && destroy;
        if (!ok && error) {
            *error = QStringLiteral("stemtex-renderer.dll does not export the expected renderer ABI.");
        }
        return ok;
    }
};

class StemtexRendererSession
{
public:
    ~StemtexRendererSession()
    {
        if (m_renderer && m_api.destroy) {
            m_api.destroy(m_renderer);
        }
    }

    static void prewarm()
    {
        (void)sharedSession(false, false, nullptr);
    }

    static void reset()
    {
        std::unique_ptr<StemtexRendererSession> oldSession;
        SharedState &state = sharedState();
        {
            std::lock_guard<std::mutex> guard(state.mutex);
            ++state.generation;
            oldSession = std::move(state.session);
            state.initializing = false;
            state.attempted = false;
            state.lastError.clear();
        }
        state.condition.notify_all();
    }

    static StemtexRendererSession *instance(QString *error, bool waitForInitialization = false)
    {
        return sharedSession(true, waitForInitialization, error);
    }

    static StemTeXStatus sharedStatus()
    {
        StemTeXStatus status;
        status.supported = true;
        SharedState &state = sharedState();
        std::lock_guard<std::mutex> guard(state.mutex);
        if (state.session) {
            return state.session->status();
        }

        status.initializing = state.initializing;
        if (!state.lastError.isEmpty()) {
            status.note = state.lastError;
        } else if (state.initializing) {
            status.note = i18n("StemTeX renderer is starting.");
        } else if (!state.attempted) {
            status.note = i18n("StemTeX renderer has not started yet.");
        }
        return status;
    }

    static QStringList availableProfileNames()
    {
        const QString runtime = runtimeRoot();
        const QString dllPath = rendererDllPath(runtime);
        if (!QFileInfo::exists(dllPath)) {
            return {};
        }

        configureRendererDllSearch(runtime);
        StemtexApi api(dllPath);
        QString error;
        if (!api.load(&error)) {
            return {};
        }

        QStringList names;
        QString lastError;
        const QList<StemTeXProfileInfo> profiles = availableProfiles(api, &lastError);
        for (const StemTeXProfileInfo &profile : profiles) {
            names << profile.name;
        }
        names.removeDuplicates();
        return names;
    }

    static QString defaultTexmfRoot()
    {
        return runtimeRoot();
    }

    LatexRenderer::Error render(const QString &latexSource,
                                const QColor &textColor,
                                double maxWidth,
                                QString &pdfFileName,
                                QString &latexOutput,
                                QStringList &fileList,
                                LatexRenderWarning *warning)
    {
        if (!m_renderer) {
            latexOutput = i18n("StemTeX renderer is not initialized.");
            return LatexRenderer::LatexFailed;
        }

        const int widthPt = std::isfinite(maxWidth) && maxWidth > 0.0 ? qRound(maxWidth) : 360;
        const QColor effectiveColor = textColor.isValid() ? textColor : QColor(Qt::black);
        const QString coloredSource = QStringLiteral("{\\color[rgb]{%1,%2,%3}\n%4\n\\par}")
                                          .arg(effectiveColor.redF(), 0, 'f', 6)
                                          .arg(effectiveColor.greenF(), 0, 'f', 6)
                                          .arg(effectiveColor.blueF(), 0, 'f', 6)
                                          .arg(latexSource);
        const QByteArray snippet = coloredSource.toUtf8();

        struct RenderState {
            std::mutex mutex;
            std::condition_variable condition;
            bool done = false;
            int ok = 0;
            int errorCode = 0;
            uint64_t jobId = 0;
            uint64_t callbackJobId = 0;
            QString errorText;
            QString sourcePdfFile;
            QString summary;
            int outcomeCode = StemTeXRenderOutcomeOk;
            QString outcomeMessage;
        };
        auto state = std::make_shared<RenderState>();

        const auto callback = [](uint64_t jobId, int ok, const StemTeXRenderResult *result, int errorCode, const char *error, void *userData) {
            auto *state = static_cast<RenderState *>(userData);
            std::lock_guard<std::mutex> guard(state->mutex);
            state->callbackJobId = jobId;
            state->ok = ok;
            state->errorCode = errorCode;
            state->errorText = error ? QString::fromUtf8(error) : QString();
            state->sourcePdfFile = result && result->pdf_path_utf8 ? QString::fromUtf8(result->pdf_path_utf8) : QString();
            state->summary = result && result->summary_json_utf8 ? QString::fromUtf8(result->summary_json_utf8) : QString();
            state->outcomeCode = result ? result->outcome_code : errorCode;
            state->outcomeMessage = result && result->outcome_message_utf8 ? QString::fromUtf8(result->outcome_message_utf8) : QString();
            state->done = true;
            state->condition.notify_all();
        };

        int submitErrorCode = 0;
        char *submitError = nullptr;
        uint64_t jobId = 0;
        const int submitted = m_api.renderAsync(m_renderer, snippet.constData(), widthPt, &jobId, callback, state.get(), &submitErrorCode, &submitError);
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            state->jobId = jobId;
        }
        if (!submitted) {
            const QString errorText = submitError ? QString::fromUtf8(submitError) : QString();
            if (submitError) {
                m_api.freeString(submitError);
            }
            latexOutput = i18n("StemTeX rendering failed: %1", errorText.isEmpty() ? QString::number(submitErrorCode) : errorText);
            return LatexRenderer::LatexFailed;
        }
        if (submitError) {
            m_api.freeString(submitError);
        }

        {
            std::unique_lock<std::mutex> guard(state->mutex);
            state->condition.wait(guard, [&state]() {
                return state->done;
            });
        }

        int ok = 0;
        int errorCode = 0;
        uint64_t callbackJobId = 0;
        QString errorText;
        QString sourcePdfFile;
        QString summary;
        int outcomeCode = StemTeXRenderOutcomeOk;
        QString outcomeMessage;
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            ok = state->ok;
            errorCode = state->errorCode;
            callbackJobId = state->callbackJobId;
            errorText = state->errorText;
            sourcePdfFile = state->sourcePdfFile;
            summary = state->summary;
            outcomeCode = state->outcomeCode;
            outcomeMessage = state->outcomeMessage;
        }

        if (callbackJobId != jobId) {
            latexOutput = i18n("StemTeX rendering failed: stale async job result.");
            return LatexRenderer::LatexFailed;
        }
        if (!ok || sourcePdfFile.isEmpty() || !QFileInfo::exists(sourcePdfFile)) {
            latexOutput = i18n("StemTeX rendering failed: %1", errorText.isEmpty() ? QString::number(errorCode) : errorText);
            return LatexRenderer::LatexFailed;
        }

        latexOutput = summary;
        pdfFileName = sourcePdfFile;
        fileList << sourcePdfFile;
        if (warning && outcomeCode != StemTeXRenderOutcomeOk) {
            warning->type = LatexRenderWarningType::CompileError;
            warning->message = outcomeMessage.trimmed();
            if (warning->message.isEmpty()) {
                warning->message = i18n("StemTeX returned code %1. The rendered PDF is still shown.", outcomeCode);
            }
            warning->severity = 1.0;
        }
        qCDebug(OkularUiDebug) << "StemTeX render finished; PDF:" << pdfFileName << "outcome:" << outcomeCode << "message:" << outcomeMessage << "summary:" << summary;
        return LatexRenderer::NoError;
    }

private:
    struct SharedState {
        std::mutex mutex;
        std::condition_variable condition;
        std::unique_ptr<StemtexRendererSession> session;
        uint64_t generation = 0;
        bool initializing = false;
        bool attempted = false;
        QString lastError;
    };

    StemtexRendererSession(QString runtimeRoot, QString dllPath)
        : m_runtimeRoot(std::move(runtimeRoot))
        , m_dllPath(std::move(dllPath))
        , m_api(m_dllPath)
    {
    }

    static StemtexRendererSession *sharedSession(bool reportStarting, bool waitForInitialization, QString *error)
    {
        SharedState &state = sharedState();
        bool startInitialization = false;
        uint64_t generation = 0;

        {
            std::unique_lock<std::mutex> guard(state.mutex);
            if (state.session) {
                return state.session.get();
            }
            if (!state.initializing && !state.attempted) {
                state.initializing = true;
                startInitialization = true;
                generation = state.generation;
            } else if (state.initializing && !waitForInitialization) {
                if (error && reportStarting) {
                    *error = i18n("StemTeX renderer is still starting.");
                }
                return nullptr;
            } else if (state.attempted) {
                if (error) {
                    *error = state.lastError;
                }
                return nullptr;
            }
        }

        if (startInitialization) {
            std::thread([generation]() {
                QString initError;
                const QString runtime = runtimeRoot();
                std::unique_ptr<StemtexRendererSession> created(new StemtexRendererSession(runtime, rendererDllPath(runtime)));
                const bool ok = created->initialize(&initError);

                SharedState &state = sharedState();
                {
                    std::lock_guard<std::mutex> guard(state.mutex);
                    if (generation == state.generation) {
                        if (ok) {
                            state.session = std::move(created);
                            state.lastError.clear();
                        } else {
                            state.lastError = initError;
                        }
                        state.initializing = false;
                        state.attempted = true;
                    }
                }
                state.condition.notify_all();
            }).detach();
        }

        if (waitForInitialization) {
            std::unique_lock<std::mutex> guard(state.mutex);
            state.condition.wait(guard, [&state]() {
                return !state.initializing;
            });
            if (state.session) {
                return state.session.get();
            }
            if (error) {
                *error = state.lastError;
            }
            return nullptr;
        }

        if (error && reportStarting) {
            *error = i18n("StemTeX renderer is starting.");
        }
        return nullptr;
    }

    static SharedState &sharedState()
    {
        static SharedState state;
        return state;
    }

    static QString environmentPath(const char *name)
    {
        const QString value = QString::fromLocal8Bit(qgetenv(name)).trimmed();
        if (value.isEmpty()) {
            return {};
        }
        return QDir::cleanPath(QDir(value).absolutePath());
    }

    static QString normalizeRuntimeRoot(const QString &path)
    {
        QDir dir(QDir::cleanPath(QDir(path).absolutePath()));
        const QString nestedRuntime = dir.filePath(QStringLiteral("runtime"));
        if (QFileInfo::exists(QDir(nestedRuntime).filePath(QStringLiteral("bin/windows/xetexdaemon.exe")))) {
            return QDir::cleanPath(nestedRuntime);
        }
        return QDir::cleanPath(dir.absolutePath());
    }

    static QString runtimeRoot()
    {
        const QString envRuntime = environmentPath("SCHOLIA_STEMTEX_RUNTIME_ROOT");
        if (!envRuntime.isEmpty()) {
            return normalizeRuntimeRoot(envRuntime);
        }

        return normalizeRuntimeRoot(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../StemTeX/runtime")));
    }

    static QString missingRuntimeComponent(const QString &runtimeRoot)
    {
        const QDir runtimeDir(runtimeRoot);
        for (const QString &relativePath : {
                 QStringLiteral("run-xelatexdaemon.bat"),
                 QStringLiteral("bin/windows/stemtex-worker-host.exe"),
                 QStringLiteral("bin/windows/xetexdaemon.exe"),
                 QStringLiteral("bin/windows/xdvipdfmxdaemon.exe"),
                 QStringLiteral("bin/windows/dvipdfmxdaemon.dll"),
                 QStringLiteral("texmf-var/web2c/xetex/xelatexdaemon.fmt"),
             }) {
            if (!QFileInfo::exists(runtimeDir.filePath(relativePath))) {
                return relativePath;
            }
        }
        return {};
    }

    static QString texmfRoot(const QString &runtimeRoot)
    {
        const QString envTexmf = environmentPath("SCHOLIA_STEMTEX_TEXMF_ROOT");
        if (!envTexmf.isEmpty()) {
            return envTexmf;
        }
        const QString configuredTexmf = Okular::Settings::latexStemtexTexmfRoot().trimmed();
        return configuredTexmf.isEmpty() ? runtimeRoot : QDir::cleanPath(QDir(configuredTexmf).absolutePath());
    }

    static QString profilesRoot()
    {
        const QString envProfiles = environmentPath("SCHOLIA_STEMTEX_PROFILES_ROOT");
        if (!envProfiles.isEmpty()) {
            return envProfiles;
        }
        return QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../StemTeX/gui/profiles")));
    }

    static QString writableStemTeXTempRoot()
    {
        QString temp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (temp.isEmpty()) {
            temp = QDir::tempPath();
        }
        return QDir::cleanPath(QDir(temp).filePath(QStringLiteral("scholia/StemTeX")));
    }

    static bool readProfileInfo(const StemtexApi &api, const QString &profileRoot, StemTeXProfileInfo *profileInfo, QString *error)
    {
        if (profileRoot.isEmpty()) {
            if (error) {
                *error = i18n("StemTeX profile path is empty.");
            }
            return false;
        }

        QByteArray profile = QDir::cleanPath(profileRoot).toUtf8();
        int errorCode = 0;
        char *errorUtf8 = nullptr;
        char *json = api.profileInfo(profile.constData(), &errorCode, &errorUtf8);
        const QString errorText = errorUtf8 ? QString::fromUtf8(errorUtf8) : QString();
        QByteArray jsonBytes;
        if (json) {
            jsonBytes = QByteArray(json);
            api.freeString(json);
        }
        if (errorUtf8) {
            api.freeString(errorUtf8);
        }
        if (!json) {
            if (error) {
                *error = i18n("StemTeX profile is not valid: %1", errorText.isEmpty() ? QString::number(errorCode) : errorText);
            }
            return false;
        }

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(jsonBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (error) {
                *error = i18n("StemTeX profile metadata is not valid JSON: %1", parseError.errorString());
            }
            return false;
        }
        if (!document.object().value(QLatin1String("valid")).toBool()) {
            if (error) {
                *error = i18n("StemTeX profile is not valid: %1", QDir::toNativeSeparators(profileRoot));
            }
            return false;
        }
        const QJsonObject object = document.object();
        QString path = object.value(QLatin1String("path")).toString();
        if (path.isEmpty()) {
            path = QDir::cleanPath(profileRoot);
        }
        QString name = object.value(QLatin1String("name")).toString();
        if (name.isEmpty()) {
            name = QFileInfo(path).fileName();
        }
        if (profileInfo) {
            profileInfo->name = name;
            profileInfo->path = QDir::cleanPath(path);
        }
        return true;
    }

    static QList<StemTeXProfileInfo> availableProfiles(const StemtexApi &api, QString *lastError)
    {
        QList<StemTeXProfileInfo> profiles;
        const QString root = profilesRoot();
        const QDir profilesDir(root);
        if (!profilesDir.exists()) {
            if (lastError) {
                *lastError = i18n("StemTeX profiles directory was not found: %1", QDir::toNativeSeparators(root));
            }
            return profiles;
        }

        const QFileInfoList candidates = profilesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &candidate : candidates) {
            StemTeXProfileInfo profile;
            if (readProfileInfo(api, candidate.absoluteFilePath(), &profile, lastError)) {
                profiles << profile;
            }
        }
        return profiles;
    }

    static QString selectProfileRoot(const StemtexApi &api, QString *error)
    {
        const QString explicitProfileRoot = environmentPath("SCHOLIA_STEMTEX_PROFILE_ROOT");
        if (!explicitProfileRoot.isEmpty()) {
            StemTeXProfileInfo explicitProfile;
            return readProfileInfo(api, explicitProfileRoot, &explicitProfile, error) ? explicitProfile.path : QString();
        }

        QStringList preferredNames;
        const QString envProfileName = QString::fromLocal8Bit(qgetenv("SCHOLIA_STEMTEX_PROFILE_NAME")).trimmed();
        const QString configuredProfileName = Okular::Settings::latexStemtexProfileName().trimmed();
        if (!envProfileName.isEmpty()) {
            preferredNames << envProfileName;
        } else if (!configuredProfileName.isEmpty()) {
            preferredNames << configuredProfileName;
        }
        preferredNames << QStringLiteral("unicodemath_cjk") << QStringLiteral("physics_cjk") << QStringLiteral("cjk_math_light") << QStringLiteral("math_light") << QStringLiteral("stem_units")
                       << QStringLiteral("chemistry") << QStringLiteral("unicodemath");
        preferredNames.removeDuplicates();

        QString lastError;
        const QList<StemTeXProfileInfo> profiles = availableProfiles(api, &lastError);
        for (const QString &name : preferredNames) {
            for (const StemTeXProfileInfo &profile : profiles) {
                if (profile.name.compare(name, Qt::CaseInsensitive) == 0 || QFileInfo(profile.path).fileName().compare(name, Qt::CaseInsensitive) == 0) {
                    return profile.path;
                }
            }
        }
        if (!profiles.isEmpty()) {
            return profiles.first().path;
        }

        if (error) {
            const QString root = profilesRoot();
            *error = lastError.isEmpty() ? i18n("No valid StemTeX profile was found in %1", QDir::toNativeSeparators(root)) : lastError;
        }
        return {};
    }

    static QString rendererDllPath(const QString &runtimeRoot)
    {
        const QString envDll = QString::fromLocal8Bit(qgetenv("STEMTEX_RENDERER_DLL")).trimmed();
        if (!envDll.isEmpty()) {
            return QDir::cleanPath(envDll);
        }
        return QDir(runtimeRoot).filePath(QStringLiteral("bin/sdk/stemtex-renderer.dll"));
    }

    static void configureRendererDllSearch(const QString &runtimeRoot)
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString sdkDir = QDir(runtimeRoot).filePath(QStringLiteral("bin/sdk"));
        const std::wstring appDirWide = QDir::toNativeSeparators(QDir::cleanPath(appDir)).toStdWString();
        const std::wstring sdkDirWide = QDir::toNativeSeparators(QDir::cleanPath(sdkDir)).toStdWString();
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
        AddDllDirectory(appDirWide.c_str());
        AddDllDirectory(sdkDirWide.c_str());
    }

    bool initialize(QString *error)
    {
        if (!QFileInfo::exists(m_dllPath)) {
            if (error) {
                *error = i18n("StemTeX renderer DLL was not found: %1", QDir::toNativeSeparators(m_dllPath));
            }
            return false;
        }
        const QString missingComponent = missingRuntimeComponent(m_runtimeRoot);
        if (!missingComponent.isEmpty()) {
            if (error) {
                *error = i18n("StemTeX runtime is incomplete: missing %1 under %2", QDir::toNativeSeparators(missingComponent), QDir::toNativeSeparators(m_runtimeRoot));
            }
            return false;
        }
        configureRendererDllSearch(m_runtimeRoot);
        if (!m_api.load(error)) {
            return false;
        }

        QString profileError;
        const QString profile = selectProfileRoot(m_api, &profileError);
        if (profile.isEmpty()) {
            if (error) {
                *error = profileError;
            }
            return false;
        }

        const QByteArray runtime = QDir::cleanPath(m_runtimeRoot).toUtf8();
        const QString texmfRootPath = texmfRoot(m_runtimeRoot);
        const QByteArray texmf = QDir::cleanPath(texmfRootPath).toUtf8();
        const QByteArray profileRoot = QDir::cleanPath(profile).toUtf8();
        const QString tempRoot = writableStemTeXTempRoot();
        const QByteArray stateRoot = QDir(tempRoot).filePath(QStringLiteral("state")).toUtf8();
        const QByteArray rendersRoot = QDir(tempRoot).filePath(QStringLiteral("renders")).toUtf8();
        StemTeXConfig config{};
        config.repo_root_utf8 = runtime.constData();
        config.runtime_root_utf8 = runtime.constData();
        config.texmf_root_utf8 = texmf.constData();
        config.profile_root_utf8 = profileRoot.constData();
        config.state_root_utf8 = stateRoot.constData();
        config.renders_root_utf8 = rendersRoot.constData();
        config.request_timeout_ms = 90000;
        config.xdvipdfmx_timeout_ms = 90000;
        config.spare_worker_count = 0;
        config.delete_intermediates = 1;

        int errorCode = 0;
        char *errorUtf8 = nullptr;
        char *diagnosticsUtf8 = nullptr;
        if (!m_api.validateConfig(&config, &errorCode, &diagnosticsUtf8)) {
            const QString diagnostics = diagnosticsUtf8 ? QString::fromUtf8(diagnosticsUtf8).trimmed() : QString();
            if (diagnosticsUtf8) {
                m_api.freeString(diagnosticsUtf8);
            }
            if (error) {
                *error = i18n("StemTeX renderer configuration is invalid: %1", diagnostics.isEmpty() ? QString::number(errorCode) : diagnostics);
            }
            return false;
        }
        if (diagnosticsUtf8) {
            m_api.freeString(diagnosticsUtf8);
        }

        m_renderer = m_api.create(&config, &errorCode, &errorUtf8);
        const QString errorText = errorUtf8 ? QString::fromUtf8(errorUtf8) : QString();
        if (errorUtf8) {
            m_api.freeString(errorUtf8);
        }
        if (!m_renderer) {
            if (error) {
                *error = i18n("StemTeX renderer failed to start: %1", errorText.isEmpty() ? QString::number(errorCode) : errorText);
            }
            return false;
        }

        qCDebug(OkularUiDebug) << "StemTeX renderer initialized; runtime:" << m_runtimeRoot << "texmf:" << texmfRootPath << "profile:" << profile << "dll:" << m_dllPath;
        return true;
    }

    StemTeXStatus status()
    {
        StemTeXStatus status;
        status.supported = true;
        if (!m_renderer) {
            status.note = i18n("StemTeX renderer is not initialized.");
            return status;
        }

        StemTeXEngineSnapshot snapshot{};
        if (!m_api.engineSnapshot(m_renderer, &snapshot)) {
            status.ready = true;
            status.note = i18n("StemTeX renderer status is unavailable.");
            return status;
        }

        status.ready = true;
        status.rendererStatus = snapshot.status;
        status.renderStage = snapshot.stage;
        status.primaryReady = snapshot.primary_ready != 0;
        status.spareReady = qMax(0, snapshot.spare_ready);
        status.spareTarget = qMax(0, snapshot.spare_target);
        status.spareRebuilding = snapshot.spare_rebuilding != 0;
        status.asyncRunning = snapshot.async_running != 0;
        status.asyncPending = snapshot.async_pending != 0;
        status.runningJobId = snapshot.running_job_id;
        status.pendingJobId = snapshot.pending_job_id;
        status.lastError = snapshot.last_error;
        return status;
    }

    QString m_runtimeRoot;
    QString m_dllPath;
    StemtexApi m_api;
    void *m_renderer = nullptr;
};
#endif
LatexRenderWarning latexWarningMessage(const QString &latexOutput)
{
    Q_UNUSED(latexOutput);
    return {};
}

}

LatexRenderer::LatexRenderer()
{
}

LatexRenderer::~LatexRenderer()
{
    for (const QString &file : std::as_const(m_fileList)) {
        QFile::remove(file);
    }
}

QString LatexRenderer::lastBackendName() const
{
    return m_lastBackendName;
}

LatexRenderWarning LatexRenderer::lastWarning() const
{
    return m_lastWarning;
}

QString LatexRenderer::lastWarningMessage() const
{
    return m_lastWarning.message;
}

LatexRenderer::Error LatexRenderer::renderLatexInHtml(QString &html, const QColor &textColor, int fontSize, int resolution, QString &latexOutput)
{
    Q_UNUSED(textColor);
    Q_UNUSED(fontSize);
    Q_UNUSED(resolution);

    m_lastBackendName.clear();
    m_lastWarning = {};

    if (!html.contains(QStringLiteral("$$"))) {
        return NoError;
    }

    latexOutput = i18n("Legacy popup formula rendering has been removed. Use %1 LaTeX notes backed by StemTeX.", QStringLiteral("Scholia"));
    return LatexFailed;
}

bool LatexRenderer::mightContainLatex(const QString &text)
{
    Q_UNUSED(text);
    return false;
}

QString LatexRenderer::compactErrorMessage(const QString &latexOutput)
{
    const QStringList lines = latexOutput.split(QLatin1Char('\n'));
    QString message;
    QString context;
    bool foundErrorLine = false;

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (!foundErrorLine && trimmed.startsWith(QLatin1Char('!'))) {
            message = trimmed.mid(1).trimmed();
            foundErrorLine = true;
            continue;
        }
        if (foundErrorLine && trimmed.startsWith(QLatin1String("l."))) {
            context = trimmed;
            break;
        }
        if (message.isEmpty()) {
            message = trimmed;
        }
    }

    if (message.isEmpty()) {
        message = i18n("Unknown LaTeX error.");
    }
    if (!context.isEmpty()) {
        message = i18nc("LaTeX error with compiler context", "%1 (%2)", message, context);
    }

    message = message.simplified();
    constexpr int maxLength = 180;
    if (message.size() > maxLength) {
        message = message.left(maxLength - 3) + QStringLiteral("...");
    }
    return message;
}

void LatexRenderer::prewarmStemTeX()
{
#ifdef Q_OS_WIN
    StemtexRendererSession::prewarm();
#endif
}

QStringList LatexRenderer::stemTeXProfileNames()
{
#ifdef Q_OS_WIN
    return StemtexRendererSession::availableProfileNames();
#else
    return {};
#endif
}

QString LatexRenderer::defaultStemTeXTexmfRoot()
{
#ifdef Q_OS_WIN
    return StemtexRendererSession::defaultTexmfRoot();
#else
    return {};
#endif
}

void LatexRenderer::restartStemTeX()
{
#ifdef Q_OS_WIN
    StemtexRendererSession::reset();
    StemtexRendererSession::prewarm();
#endif
}

StemTeXStatus LatexRenderer::stemTeXStatus()
{
#ifdef Q_OS_WIN
    return StemtexRendererSession::sharedStatus();
#else
    StemTeXStatus status;
    status.note = i18n("StemTeX renderer is only available on Windows.");
    return status;
#endif
}

LatexRenderer::Error LatexRenderer::renderLatexToImage(const QString &latexFormula, const QColor &textColor, int fontSize, int resolution, QString &fileName, QString &latexOutput)
{
    Q_UNUSED(latexFormula);
    Q_UNUSED(textColor);
    Q_UNUSED(fontSize);
    Q_UNUSED(resolution);

    m_lastBackendName.clear();
    m_lastWarning = {};
    fileName.clear();
    latexOutput = i18n("Legacy LaTeX image rendering has been removed. Use %1 LaTeX notes backed by StemTeX.", QStringLiteral("Scholia"));
    return LatexFailed;
}

LatexRenderer::Error LatexRenderer::renderLatexToPdf(const QString &latexFormula, const QColor &textColor, QString &pdfFileName, QString &latexOutput, double maxWidth)
{
    m_lastBackendName.clear();
    m_lastWarning = {};

    QString formula = latexFormula.trimmed();
    if (formula.isEmpty()) {
        pdfFileName.clear();
        return LatexFailed;
    }
    if (!securityCheck(formula)) {
        pdfFileName.clear();
        latexOutput = i18n("The formula contains unsupported LaTeX commands.");
        return LatexFailed;
    }

#ifdef Q_OS_WIN
    logTexInvocation("stemtex-render",
                     QStringLiteral("stemtex"),
                     QStringLiteral("configured-stemtex"),
                     {QStringLiteral("max width: %1").arg(maxWidth),
                      QStringLiteral("source length: %1").arg(formula.size())});
    QString stemtexError;
    const bool waitForStemTeXStartup = QCoreApplication::instance() && QThread::currentThread() != QCoreApplication::instance()->thread();
    StemtexRendererSession *session = StemtexRendererSession::instance(&stemtexError, waitForStemTeXStartup);
    if (!session) {
        pdfFileName.clear();
        latexOutput = stemtexError.isEmpty() ? i18n("StemTeX renderer is not available.") : stemtexError;
        return LatexFailed;
    }

    const Error stemtexErrorCode = session->render(formula, textColor, maxWidth, pdfFileName, latexOutput, m_fileList, &m_lastWarning);
    if (stemtexErrorCode == NoError) {
        m_lastBackendName = QStringLiteral("stemtex");
    }
    return stemtexErrorCode;
#else
    pdfFileName.clear();
    latexOutput = i18n("StemTeX rendering is only available on Windows.");
    return LatexFailed;
#endif
}

bool LatexRenderer::securityCheck(const QString &latexFormula)
{
    static const auto formulaRegex =
        QRegularExpression(QString::fromLatin1("\\\\(def|let|futurelet|newcommand|renewcommand|else|fi|write|input|include"
                                               "|chardef|catcode|makeatletter|noexpand|toksdef|every|errhelp|errorstopmode|scrollmode|nonstopmode|batchmode"
                                               "|read|csname|newhelp|relax|afterground|afterassignment|expandafter|noexpand|special|command|loop|repeat|toks"
                                               "|output|line|mathcode|name|item|section|mbox|DeclareRobustCommand)[^a-zA-Z]"));
    return !latexFormula.contains(formulaRegex);
}

}
