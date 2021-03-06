/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qopengl.h"
#include "qopengl_p.h"

#include "qopenglcontext.h"
#include "qopenglfunctions.h"
#include "qoffscreensurface.h"

#include <QtCore/QDebug>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QDir>

QT_BEGIN_NAMESPACE

#if defined(QT_OPENGL_3)
typedef const GLubyte * (QOPENGLF_APIENTRYP qt_glGetStringi)(GLenum, GLuint);
#endif

QOpenGLExtensionMatcher::QOpenGLExtensionMatcher()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    QOpenGLFunctions *funcs = ctx->functions();
    const char *extensionStr = 0;

    if (ctx->isOpenGLES() || ctx->format().majorVersion() < 3)
        extensionStr = reinterpret_cast<const char *>(funcs->glGetString(GL_EXTENSIONS));

    if (extensionStr) {
        QByteArray ba(extensionStr);
        QList<QByteArray> extensions = ba.split(' ');
        m_extensions = extensions.toSet();
    } else {
#ifdef QT_OPENGL_3
        // clear error state
        while (funcs->glGetError()) {}

        if (ctx) {
            qt_glGetStringi glGetStringi = (qt_glGetStringi)ctx->getProcAddress("glGetStringi");

            if (!glGetStringi)
                return;

            GLint numExtensions;
            funcs->glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);

            for (int i = 0; i < numExtensions; ++i) {
                const char *str = reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, i));
                m_extensions.insert(str);
            }
        }
#endif // QT_OPENGL_3
    }
}

/* Helpers to read out the list of features matching a device from
 * a Chromium driver bug list. Note that not all keys are supported and
 * some may behave differently: gl_vendor is a substring match instead of regex.
 {
  "entries": [
 {
      "id": 20,
      "description": "Disable EXT_draw_buffers on GeForce GT 650M on Linux due to driver bugs",
      "os": {
        "type": "linux"
      },
      // Optional: "exceptions" list
      "vendor_id": "0x10de",
      "device_id": ["0x0fd5"],
      "multi_gpu_category": "any",
      "features": [
        "disable_ext_draw_buffers"
      ]
    },
   ....
   }
*/

QDebug operator<<(QDebug d, const QOpenGLConfig::Gpu &g)
{
    QDebugStateSaver s(d);
    d.nospace();
    d << "Gpu(";
    if (g.isValid()) {
        d << "vendor=" << hex << showbase <<g.vendorId << ", device=" << g.deviceId
          << "version=" << g.driverVersion;
    } else {
        d << 0;
    }
    d << ')';
    return d;
}

enum Operator { NotEqual, LessThan, LessEqualThan, Equals, GreaterThan, GreaterEqualThan };
static const char *operators[] = {"!=", "<", "<=", "=", ">", ">="};

static inline QString valueKey()         { return QStringLiteral("value"); }
static inline QString opKey()            { return QStringLiteral("op"); }
static inline QString versionKey()       { return QStringLiteral("version"); }
static inline QString typeKey()          { return QStringLiteral("type"); }
static inline QString osKey()            { return QStringLiteral("os"); }
static inline QString vendorIdKey()      { return QStringLiteral("vendor_id"); }
static inline QString glVendorKey()      { return QStringLiteral("gl_vendor"); }
static inline QString deviceIdKey()      { return QStringLiteral("device_id"); }
static inline QString driverVersionKey() { return QStringLiteral("driver_version"); }
static inline QString featuresKey()      { return QStringLiteral("features"); }
static inline QString idKey()            { return QStringLiteral("id"); }
static inline QString descriptionKey()   { return QStringLiteral("description"); }
static inline QString exceptionsKey()    { return QStringLiteral("exceptions"); }

namespace {
// VersionTerm describing a version term consisting of number and operator
// found in "os", "driver_version", "gl_version".
struct VersionTerm {
    VersionTerm() : op(NotEqual) {}
    static VersionTerm fromJson(const QJsonValue &v);
    bool isNull() const { return number.isNull(); }
    bool matches(const QVersionNumber &other) const;

    QVersionNumber number;
    Operator op;
};

bool VersionTerm::matches(const QVersionNumber &other) const
{
    if (isNull() || other.isNull()) {
        qWarning() << Q_FUNC_INFO << "called with invalid parameters";
        return false;
    }
    switch (op) {
    case NotEqual:
        return other != number;
    case LessThan:
        return other < number;
    case LessEqualThan:
        return other <= number;
    case Equals:
        return other == number;
    case GreaterThan:
        return other > number;
    case GreaterEqualThan:
        return other >= number;
    }
    return false;
}

VersionTerm VersionTerm::fromJson(const QJsonValue &v)
{
    VersionTerm result;
    if (!v.isObject())
        return result;
    const QJsonObject o = v.toObject();
    result.number = QVersionNumber::fromString(o.value(valueKey()).toString());
    const QString opS = o.value(opKey()).toString();
    for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); ++i) {
        if (opS == QLatin1String(operators[i])) {
            result.op = static_cast<Operator>(i);
            break;
        }
    }
    return result;
}

// OS term consisting of name and optional version found in
// under "os" in main array and in "exceptions" lists.
struct OsTypeTerm
{
    static OsTypeTerm fromJson(const QJsonValue &v);
    static QString hostOs();
    static QVersionNumber hostKernelVersion() { return QVersionNumber::fromString(QSysInfo::kernelVersion()); }

    bool isNull() const { return type.isEmpty(); }
    bool matches(const QString &osName, const QVersionNumber &kernelVersion) const
    {
        if (isNull() || osName.isEmpty() || kernelVersion.isNull()) {
            qWarning() << Q_FUNC_INFO << "called with invalid parameters";
            return false;
        }
        if (type != osName)
            return false;
        return versionTerm.isNull() || versionTerm.matches(kernelVersion);
    }

    QString type;
    VersionTerm versionTerm;
};

OsTypeTerm OsTypeTerm::fromJson(const QJsonValue &v)
{
    OsTypeTerm result;
    if (!v.isObject())
        return result;
    const QJsonObject o = v.toObject();
    result.type = o.value(typeKey()).toString();
    result.versionTerm = VersionTerm::fromJson(o.value(versionKey()));
    return result;
}

QString OsTypeTerm::hostOs()
{
    // Determine Host OS.
#if defined(Q_OS_WIN)
    return  QStringLiteral("win");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#elif defined(Q_OS_OSX)
    return  QStringLiteral("macosx");
#elif defined(Q_OS_ANDROID)
    return  QStringLiteral("android");
#else
    return QString();
#endif
}
} // anonymous namespace

typedef QJsonArray::ConstIterator JsonArrayConstIt;

static inline bool contains(const QJsonArray &a, unsigned needle)
{
    for (JsonArrayConstIt it = a.constBegin(), cend = a.constEnd(); it != cend; ++it) {
        if (needle == it->toString().toUInt(Q_NULLPTR, /* base */ 0))
            return true;
    }
    return false;
}

static QString msgSyntaxWarning(const QJsonObject &object, const QString &what)
{
    QString result;
    QTextStream(&result) << "Id " << object.value(idKey()).toInt()
        << " (\"" << object.value(descriptionKey()).toString()
        << "\"): " << what;
    return result;
}

// Check whether an entry matches. Called recursively for
// "exceptions" list.

static bool matches(const QJsonObject &object,
                    const QString &osName,
                    const QVersionNumber &kernelVersion,
                    const QOpenGLConfig::Gpu &gpu)
{
    const OsTypeTerm os = OsTypeTerm::fromJson(object.value(osKey()));
    if (!os.isNull() && !os.matches(osName, kernelVersion))
        return false;

    const QJsonValue exceptionsV = object.value(exceptionsKey());
    if (exceptionsV.isArray()) {
        const QJsonArray exceptionsA = exceptionsV.toArray();
        for (JsonArrayConstIt it = exceptionsA.constBegin(), cend = exceptionsA.constEnd(); it != cend; ++it) {
            if (matches(it->toObject(), osName, kernelVersion, gpu))
                return false;
        }
    }

    const QJsonValue vendorV = object.value(vendorIdKey());
    if (vendorV.isString()) {
        if (gpu.vendorId != vendorV.toString().toUInt(Q_NULLPTR, /* base */ 0))
            return false;
    } else {
        if (object.contains(glVendorKey())) {
            const QByteArray glVendorV = object.value(glVendorKey()).toString().toUtf8();
            if (!gpu.glVendor.contains(glVendorV))
                return false;
        }
    }

    if (gpu.deviceId) {
        const QJsonValue deviceIdV = object.value(deviceIdKey());
        switch (deviceIdV.type()) {
        case QJsonValue::Array:
            if (!contains(deviceIdV.toArray(), gpu.deviceId))
                return false;
            break;
        case QJsonValue::Undefined:
        case QJsonValue::Null:
            break;
        default:
            qWarning().noquote()
                << msgSyntaxWarning(object,
                                    QLatin1String("Device ID must be of type array."));
        }
    }
    if (!gpu.driverVersion.isNull()) {
        const QJsonValue driverVersionV = object.value(driverVersionKey());
        switch (driverVersionV.type()) {
        case QJsonValue::Object:
            if (!VersionTerm::fromJson(driverVersionV).matches(gpu.driverVersion))
                return false;
            break;
        case QJsonValue::Undefined:
        case QJsonValue::Null:
            break;
        default:
            qWarning().noquote()
                << msgSyntaxWarning(object,
                                    QLatin1String("Driver version must be of type object."));
        }
    }
    return true;
}

static bool readGpuFeatures(const QOpenGLConfig::Gpu &gpu,
                            const QString &osName,
                            const QVersionNumber &kernelVersion,
                            const QJsonDocument &doc,
                            QSet<QString> *result,
                            QString *errorMessage)
{
    result->clear();
    errorMessage->clear();
    const QJsonValue entriesV = doc.object().value(QStringLiteral("entries"));
    if (!entriesV.isArray()) {
        *errorMessage = QLatin1String("No entries read.");
        return false;
    }

    const QJsonArray entriesA = entriesV.toArray();
    for (JsonArrayConstIt eit = entriesA.constBegin(), ecend = entriesA.constEnd(); eit != ecend; ++eit) {
        if (eit->isObject()) {
            const QJsonObject object = eit->toObject();
            if (matches(object, osName, kernelVersion, gpu)) {
                const QJsonValue featuresListV = object.value(featuresKey());
                if (featuresListV.isArray()) {
                    const QJsonArray featuresListA = featuresListV.toArray();
                    for (JsonArrayConstIt fit = featuresListA.constBegin(), fcend = featuresListA.constEnd(); fit != fcend; ++fit)
                        result->insert(fit->toString());
                }
            }
        }
    }
    return true;
}

static bool readGpuFeatures(const QOpenGLConfig::Gpu &gpu,
                            const QString &osName,
                            const QVersionNumber &kernelVersion,
                            const QByteArray &jsonAsciiData,
                            QSet<QString> *result, QString *errorMessage)
{
    result->clear();
    errorMessage->clear();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonAsciiData, &error);
    if (document.isNull()) {
        const int lineNumber = 1 + jsonAsciiData.left(error.offset).count('\n');
        QTextStream str(errorMessage);
        str << "Failed to parse data: \"" << error.errorString()
            << "\" at line " << lineNumber << " (offset: "
            << error.offset << ").";
        return false;
    }
    return readGpuFeatures(gpu, osName, kernelVersion, document, result, errorMessage);
}

static bool readGpuFeatures(const QOpenGLConfig::Gpu &gpu,
                            const QString &osName,
                            const QVersionNumber &kernelVersion,
                            const QString &fileName,
                            QSet<QString> *result, QString *errorMessage)
{
    result->clear();
    errorMessage->clear();
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QTextStream str(errorMessage);
        str << "Cannot open \"" << QDir::toNativeSeparators(fileName) << "\": "
            << file.errorString();
        return false;
    }
    const bool success = readGpuFeatures(gpu, osName, kernelVersion, file.readAll(), result, errorMessage);
    if (!success) {
        errorMessage->prepend(QLatin1String("Error reading \"")
                              + QDir::toNativeSeparators(fileName)
                              + QLatin1String("\": "));
    }
    return success;
}

QSet<QString> QOpenGLConfig::gpuFeatures(const QOpenGLConfig::Gpu &gpu,
                                                  const QString &osName,
                                                  const QVersionNumber &kernelVersion,
                                                  const QJsonDocument &doc)
{
    QSet<QString> result;
    QString errorMessage;
    if (!readGpuFeatures(gpu, osName, kernelVersion, doc, &result, &errorMessage))
        qWarning().noquote() << errorMessage;
    return result;
}

QSet<QString> QOpenGLConfig::gpuFeatures(const QOpenGLConfig::Gpu &gpu,
                                                  const QString &osName,
                                                  const QVersionNumber &kernelVersion,
                                                  const QString &fileName)
{
    QSet<QString> result;
    QString errorMessage;
    if (!readGpuFeatures(gpu, osName, kernelVersion, fileName, &result, &errorMessage))
        qWarning().noquote() << errorMessage;
    return result;
}

QSet<QString> QOpenGLConfig::gpuFeatures(const Gpu &gpu, const QJsonDocument &doc)
{
    return gpuFeatures(gpu, OsTypeTerm::hostOs(), OsTypeTerm::hostKernelVersion(), doc);
}

QSet<QString> QOpenGLConfig::gpuFeatures(const Gpu &gpu, const QString &fileName)
{
    return gpuFeatures(gpu, OsTypeTerm::hostOs(), OsTypeTerm::hostKernelVersion(), fileName);
}

QOpenGLConfig::Gpu QOpenGLConfig::Gpu::fromContext()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    QScopedPointer<QOpenGLContext> tmpContext;
    QScopedPointer<QOffscreenSurface> tmpSurface;
    if (!ctx) {
        tmpContext.reset(new QOpenGLContext);
        if (!tmpContext->create()) {
            qWarning("QOpenGLConfig::Gpu::fromContext: Failed to create temporary context");
            return QOpenGLConfig::Gpu();
        }
        tmpSurface.reset(new QOffscreenSurface);
        tmpSurface->setFormat(tmpContext->format());
        tmpSurface->create();
        tmpContext->makeCurrent(tmpSurface.data());
    }

    QOpenGLConfig::Gpu gpu;
    ctx = QOpenGLContext::currentContext();
    const GLubyte *p = ctx->functions()->glGetString(GL_VENDOR);
    if (p)
        gpu.glVendor = QByteArray(reinterpret_cast<const char *>(p));

    return gpu;
}

QT_END_NAMESPACE
