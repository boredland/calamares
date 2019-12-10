/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   Copyright 2019, Adriaan de Groot <groot@kde.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TimeZone.h"

#include "utils/Logger.h"

#include <QFile>
#include <QStringList>
#include <QTextStream>

#include <cstring>

static const char TZ_DATA_FILE[] = "/usr/share/zoneinfo/zone.tab";

static double
getRightGeoLocation( QString str )
{
    double sign = 1, num = 0.00;

    // Determine sign
    if ( str.startsWith( '-' ) )
    {
        sign = -1;
        str.remove( 0, 1 );
    }
    else if ( str.startsWith( '+' ) )
    {
        str.remove( 0, 1 );
    }

    if ( str.length() == 4 || str.length() == 6 )
    {
        num = str.mid( 0, 2 ).toDouble() + str.mid( 2, 2 ).toDouble() / 60.0;
    }
    else if ( str.length() == 5 || str.length() == 7 )
    {
        num = str.mid( 0, 3 ).toDouble() + str.mid( 3, 2 ).toDouble() / 60.0;
    }

    return sign * num;
}


namespace CalamaresUtils
{
namespace Locale
{


CStringPair::CStringPair( CStringPair&& t )
    : m_human( nullptr )
    , m_key()
{
    // My pointers are initialized to nullptr
    std::swap( m_human, t.m_human );
    std::swap( m_key, t.m_key );
}

CStringPair::CStringPair( const CStringPair& t )
    : m_human( t.m_human ? strdup( t.m_human ) : nullptr )
    , m_key( t.m_key )
{
}

/** @brief Massage an identifier into a human-readable form
 *
 * Makes a copy of @p s, caller must free() it.
 */
static char*
munge( const char* s )
{
    char* t = strdup( s );
    if ( !t )
    {
        return nullptr;
    }

    // replace("_"," ") in the Python script
    char* p = t;
    while ( *p )
    {
        if ( ( *p ) == '_' )
        {
            *p = ' ';
        }
        ++p;
    }

    return t;
}

CStringPair::CStringPair( const char* s1 )
    : m_human( s1 ? munge( s1 ) : nullptr )
    , m_key( s1 ? QString( s1 ) : QString() )
{
}


CStringPair::~CStringPair()
{
    free( m_human );
}


QString
TZRegion::tr() const
{
    // NOTE: context name must match what's used in zone-extractor.py
    return QObject::tr( m_human, "tz_regions" );
}

TZRegion::~TZRegion()
{
    qDeleteAll( m_zones );
}

TZRegionList
TZRegion::fromZoneTab()
{
    return TZRegion::fromFile( TZ_DATA_FILE );
}

TZRegionList
TZRegion::fromFile( const char* fileName )
{
    TZRegionList model;

    QFile file( fileName );
    if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        return model;
    }

    QStringList regions;

    QTextStream in( &file );
    while ( !in.atEnd() )
    {
        QString line = in.readLine().trimmed().split( '#', QString::KeepEmptyParts ).first().trimmed();
        if ( line.isEmpty() )
        {
            continue;
        }

        QStringList list = line.split( QRegExp( "[\t ]" ), QString::SkipEmptyParts );
        if ( list.size() < 3 )
        {
            continue;
        }

        QStringList timezoneParts = list.at( 2 ).split( '/', QString::SkipEmptyParts );
        if ( timezoneParts.size() < 2 )
        {
            continue;
        }

        QString region = timezoneParts.first().trimmed();
        ;
        if ( region.isEmpty() )
        {
            continue;
        }

        if ( !regions.contains( region ) )
        {
            regions.append( region );
            model.append( new TZRegion( region.toUtf8().data() ) );
        }

        QString countryCode = list.at( 0 ).trimmed();
        if ( countryCode.size() != 2 )
        {
            continue;
        }

        timezoneParts.removeFirst();
        TZZone z( timezoneParts.join( '/' ).toUtf8().constData(), countryCode, list.at( 1 ) );
        cDebug() << "Region" << region << z;
    }

    std::sort( model.begin(), model.end(), []( const TZRegion* l, const TZRegion* r ) { return *l < *r; } );
    return model;
}

TZZone::TZZone( const char* zoneName, const QString& country, QString position )
    : CStringPair( zoneName )
    , m_country( country )
{
    int cooSplitPos = position.indexOf( QRegExp( "[-+]" ), 1 );
    if ( cooSplitPos > 0 )
    {
        m_latitude = getRightGeoLocation( position.mid( 0, cooSplitPos ) );
        m_longitude = getRightGeoLocation( position.mid( cooSplitPos + 1 ) );
    }
}

QString
TZZone::tr() const
{
    // NOTE: context name must match what's used in zone-extractor.py
    return QObject::tr( m_human, "tz_names" );
}

void
TZZone::print( QDebug& log ) const
{
    log << key() << '(' << m_country << ' ' << m_latitude << ',' << m_longitude << ')';
}


TZRegionModel::TZRegionModel( TZRegionList l )
    : m_regions( l )
{
}

TZRegionModel::~TZRegionModel() {}

int
TZRegionModel::rowCount( const QModelIndex& parent ) const
{
    return m_regions.count();
}

QVariant
TZRegionModel::data( const QModelIndex& index, int role ) const
{
    if ( ( role != Qt::DisplayRole ) && ( role != Qt::UserRole ) )
    {
        return QVariant();
    }

    if ( !index.isValid() )
    {
        return QVariant();
    }

    const TZRegion* region = m_regions.at( index.row() );
    return role == Qt::DisplayRole ? region->tr() : region->key();
}

const TZRegion*
TZRegionModel::region( int index ) const
{
    if ( ( index < 0 ) || ( index >= m_regions.count() ) )
    {
        return nullptr;
    }
    return m_regions[ index ];
}

}  // namespace Locale
}  // namespace CalamaresUtils
