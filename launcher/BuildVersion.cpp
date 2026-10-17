#include "BuildVersion.hpp"

#include <QDate>
#include <QLocale>

#include "BuildInfo.hpp"

QString BuildVersion()
{
    const QDate date = QDate::fromString(LCE_BUILD_DATE, Qt::ISODate);
    const QString built = date.isValid() ? QLocale().toString(date, "d MMM yyyy") : QString(LCE_BUILD_DATE);
    return QString("%1%2  ·  %3").arg(LCE_BUILD_COMMIT, LCE_BUILD_DIRTY ? " (modified)" : "", built);
}
