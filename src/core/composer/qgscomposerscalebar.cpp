/***************************************************************************
                           qgscomposerscalebar.cpp
                             -------------------
    begin                : March 2005
    copyright            : (C) 2005 by Radim Blazek
    email                : blazek@itc.it
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgscomposerscalebar.h"
#include "qgscomposermap.h"
#include "qgscomposition.h"
#include "qgscomposerutils.h"
#include "qgsdistancearea.h"
#include "qgsscalebarstyle.h"
#include "qgsdoubleboxscalebarstyle.h"
#include "qgsmapsettings.h"
#include "qgsnumericscalebarstyle.h"
#include "qgssingleboxscalebarstyle.h"
#include "qgsticksscalebarstyle.h"
#include "qgsrectangle.h"
#include "qgsproject.h"
#include "qgssymbollayerutils.h"
#include "qgsfontutils.h"
#include "qgsunittypes.h"
#include <QDomDocument>
#include <QDomElement>
#include <QFontMetricsF>
#include <QPainter>
#include <QSettings>
#include <cmath>

QgsComposerScaleBar::QgsComposerScaleBar( QgsComposition* composition )
    : QgsComposerItem( composition )
    , mComposerMap( nullptr )
    , mNumUnitsPerSegment( 0 )
    , mSegmentSizeMode( SegmentSizeFixed )
    , mMinBarWidth( 50 )
    , mMaxBarWidth( 150 )
    , mFontColor( QColor( 0, 0, 0 ) )
    , mStyle( nullptr )
    , mSegmentMillimeters( 0.0 )
    , mAlignment( Left )
    , mUnits( MapUnits )
    , mLineJoinStyle( Qt::MiterJoin )
    , mLineCapStyle( Qt::SquareCap )
{
  applyDefaultSettings();
  applyDefaultSize();
}

QgsComposerScaleBar::~QgsComposerScaleBar()
{
  delete mStyle;
}

void QgsComposerScaleBar::paint( QPainter* painter, const QStyleOptionGraphicsItem* itemStyle, QWidget* pWidget )
{
  Q_UNUSED( itemStyle );
  Q_UNUSED( pWidget );
  if ( !mStyle || !painter )
  {
    return;
  }
  if ( !shouldDrawItem() )
  {
    return;
  }

  drawBackground( painter );

  //x-offset is half of first label width because labels are drawn centered
  QString firstLabel = firstLabelString();
  double firstLabelWidth = QgsComposerUtils::textWidthMM( mFont, firstLabel );

  mStyle->draw( painter, firstLabelWidth / 2 );

  //draw frame and selection boxes if necessary
  drawFrame( painter );
  if ( isSelected() )
  {
    drawSelectionBoxes( painter );
  }
}

void QgsComposerScaleBar::setNumSegments( int nSegments )
{
  if ( !mStyle )
  {
    mNumSegments = nSegments;
    return;
  }
  double width = mStyle->calculateBoxSize().width();
  mNumSegments = nSegments;
  double widthAfter = mStyle->calculateBoxSize().width();
  correctXPositionAlignment( width, widthAfter );
  emit itemChanged();
}

void QgsComposerScaleBar::setNumUnitsPerSegment( double units )
{
  if ( !mStyle )
  {
    mNumUnitsPerSegment = units;
    return;
  }
  double width = mStyle->calculateBoxSize().width();
  mNumUnitsPerSegment = units;
  refreshSegmentMillimeters();
  double widthAfter = mStyle->calculateBoxSize().width();
  correctXPositionAlignment( width, widthAfter );
  emit itemChanged();
}

void QgsComposerScaleBar::setSegmentSizeMode( SegmentSizeMode mode )
{
  if ( !mStyle )
  {
    mSegmentSizeMode = mode;
    return;
  }
  double width = mStyle->calculateBoxSize().width();
  mSegmentSizeMode = mode;
  refreshSegmentMillimeters();
  double widthAfter = mStyle->calculateBoxSize().width();
  correctXPositionAlignment( width, widthAfter );
  emit itemChanged();
}

void QgsComposerScaleBar::setMinBarWidth( double minWidth )
{
  if ( !mStyle )
  {
    mMinBarWidth = minWidth;
    return;
  }
  double width = mStyle->calculateBoxSize().width();
  mMinBarWidth = minWidth;
  refreshSegmentMillimeters();
  double widthAfter = mStyle->calculateBoxSize().width();
  correctXPositionAlignment( width, widthAfter );
  emit itemChanged();
}

void QgsComposerScaleBar::setMaxBarWidth( double maxWidth )
{
  if ( !mStyle )
  {
    mMaxBarWidth = maxWidth;
    return;
  }
  double width = mStyle->calculateBoxSize().width();
  mMaxBarWidth = maxWidth;
  refreshSegmentMillimeters();
  double widthAfter = mStyle->calculateBoxSize().width();
  correctXPositionAlignment( width, widthAfter );
  emit itemChanged();
}

void QgsComposerScaleBar::setNumSegmentsLeft( int nSegmentsLeft )
{
  if ( !mStyle )
  {
    mNumSegmentsLeft = nSegmentsLeft;
    return;
  }
  double width = mStyle->calculateBoxSize().width();
  mNumSegmentsLeft = nSegmentsLeft;
  double widthAfter = mStyle->calculateBoxSize().width();
  correctXPositionAlignment( width, widthAfter );
  emit itemChanged();
}

void QgsComposerScaleBar::setBoxContentSpace( double space )
{
  if ( !mStyle )
  {
    mBoxContentSpace = space;
    return;
  }
  double width = mStyle->calculateBoxSize().width();
  mBoxContentSpace = space;
  double widthAfter = mStyle->calculateBoxSize().width();
  correctXPositionAlignment( width, widthAfter );
  emit itemChanged();
}

void QgsComposerScaleBar::setComposerMap( const QgsComposerMap* map )
{
  if ( mComposerMap )
  {
    disconnect( mComposerMap, SIGNAL( extentChanged() ), this, SLOT( updateSegmentSize() ) );
    disconnect( mComposerMap, SIGNAL( destroyed( QObject* ) ), this, SLOT( invalidateCurrentMap() ) );
  }
  mComposerMap = map;

  if ( !map )
  {
    return;
  }

  connect( mComposerMap, SIGNAL( extentChanged() ), this, SLOT( updateSegmentSize() ) );
  connect( mComposerMap, SIGNAL( destroyed( QObject* ) ), this, SLOT( invalidateCurrentMap() ) );

  refreshSegmentMillimeters();
  emit itemChanged();
}

void QgsComposerScaleBar::invalidateCurrentMap()
{
  if ( !mComposerMap )
  {
    return;
  }

  disconnect( mComposerMap, SIGNAL( extentChanged() ), this, SLOT( updateSegmentSize() ) );
  disconnect( mComposerMap, SIGNAL( destroyed( QObject* ) ), this, SLOT( invalidateCurrentMap() ) );
  mComposerMap = nullptr;
}

// nextNiceNumber(4573.23, d) = 5000 (d=1) -> 4600 (d=10) -> 4580 (d=100) -> 4574 (d=1000) -> etc
inline double nextNiceNumber( double a, double d = 1 )
{
  double s = qPow( 10.0, floor( log10( a ) ) ) / d;
  return ceil( a / s ) * s;
}

// prevNiceNumber(4573.23, d) = 4000 (d=1) -> 4500 (d=10) -> 4570 (d=100) -> 4573 (d=1000) -> etc
inline double prevNiceNumber( double a, double d = 1 )
{
  double s = qPow( 10.0, floor( log10( a ) ) ) / d;
  return floor( a / s ) * s;
}

void QgsComposerScaleBar::refreshSegmentMillimeters()
{
  if ( mComposerMap )
  {
    //get mm dimension of composer map
    QRectF composerItemRect = mComposerMap->rect();

    if ( mSegmentSizeMode == SegmentSizeFixed )
    {
      //calculate size depending on mNumUnitsPerSegment
      mSegmentMillimeters = composerItemRect.width() / mapWidth() * mNumUnitsPerSegment;
    }
    else /*if(mSegmentSizeMode == SegmentSizeFitWidth)*/
    {
      if ( mMaxBarWidth < mMinBarWidth )
      {
        mSegmentMillimeters = 0;
      }
      else
      {
        double nSegments = ( mNumSegmentsLeft != 0 ) + mNumSegments;
        // unitsPerSegments which fit minBarWidth resp. maxBarWidth
        double minUnitsPerSeg = ( mMinBarWidth * mapWidth() ) / ( nSegments * composerItemRect.width() );
        double maxUnitsPerSeg = ( mMaxBarWidth * mapWidth() ) / ( nSegments * composerItemRect.width() );

        // Start with coarsest "nice" number closest to minUnitsPerSeg resp
        // maxUnitsPerSeg, then proceed to finer numbers as long as neither
        // lowerNiceUnitsPerSeg nor upperNiceUnitsPerSeg are are in
        // [minUnitsPerSeg, maxUnitsPerSeg]
        double lowerNiceUnitsPerSeg = nextNiceNumber( minUnitsPerSeg );
        double upperNiceUnitsPerSeg = prevNiceNumber( maxUnitsPerSeg );

        double d = 1;
        while ( lowerNiceUnitsPerSeg > maxUnitsPerSeg && upperNiceUnitsPerSeg < minUnitsPerSeg )
        {
          d *= 10;
          lowerNiceUnitsPerSeg = nextNiceNumber( minUnitsPerSeg, d );
          upperNiceUnitsPerSeg = prevNiceNumber( maxUnitsPerSeg, d );
        }

        // Pick mNumUnitsPerSegment from {lowerNiceUnitsPerSeg, upperNiceUnitsPerSeg}, use the larger if possible
        mNumUnitsPerSegment = upperNiceUnitsPerSeg < minUnitsPerSeg ? lowerNiceUnitsPerSeg : upperNiceUnitsPerSeg;
        mSegmentMillimeters = composerItemRect.width() / mapWidth() * mNumUnitsPerSegment;
      }
    }
  }
}

double QgsComposerScaleBar::mapWidth() const
{
  if ( !mComposerMap )
  {
    return 0.0;
  }

  QgsRectangle composerMapRect = *( mComposerMap->currentMapExtent() );
  if ( mUnits == MapUnits )
  {
    return composerMapRect.width();
  }
  else
  {
    QgsDistanceArea da;
    da.setEllipsoidalMode( mComposition->mapSettings().hasCrsTransformEnabled() );
    da.setSourceCrs( mComposition->mapSettings().destinationCrs().srsid() );
    da.setEllipsoid( mComposition->project()->ellipsoid() );

    QgsUnitTypes::DistanceUnit units = QgsUnitTypes::DistanceMeters;
    double measure = da.measureLine( QgsPoint( composerMapRect.xMinimum(), composerMapRect.yMinimum() ),
                                     QgsPoint( composerMapRect.xMaximum(), composerMapRect.yMinimum() ),
                                     units );
    switch ( mUnits )
    {
      case QgsComposerScaleBar::Feet:
        measure /= QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceFeet, units );
        break;
      case QgsComposerScaleBar::NauticalMiles:
        measure /= QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceNauticalMiles, units );
        break;
      case QgsComposerScaleBar::Meters:
        measure /= QgsUnitTypes::fromUnitToUnitFactor( QgsUnitTypes::DistanceMeters, units );
        break;
      case QgsComposerScaleBar::MapUnits:
        //avoid warning
        break;
    }
    return measure;
  }
}

void QgsComposerScaleBar::setAlignment( Alignment a )
{
  mAlignment = a;
  update();
  emit itemChanged();
}

void QgsComposerScaleBar::setUnits( ScaleBarUnits u )
{
  mUnits = u;
  refreshSegmentMillimeters();
  emit itemChanged();
}

void QgsComposerScaleBar::setLineJoinStyle( Qt::PenJoinStyle style )
{
  if ( mLineJoinStyle == style )
  {
    //no change
    return;
  }
  mLineJoinStyle = style;
  mPen.setJoinStyle( mLineJoinStyle );
  update();
  emit itemChanged();
}

void QgsComposerScaleBar::setLineCapStyle( Qt::PenCapStyle style )
{
  if ( mLineCapStyle == style )
  {
    //no change
    return;
  }
  mLineCapStyle = style;
  mPen.setCapStyle( mLineCapStyle );
  update();
  emit itemChanged();
}

void QgsComposerScaleBar::applyDefaultSettings()
{
  mNumSegments = 2;
  mNumSegmentsLeft = 0;

  mNumMapUnitsPerScaleBarUnit = 1.0;

  //style
  delete mStyle;
  mStyle = new QgsSingleBoxScaleBarStyle( this );

  mHeight = 3;

  //default to no background
  setBackgroundEnabled( false );

  mPen = QPen( Qt::black );
  mPen.setJoinStyle( mLineJoinStyle );
  mPen.setCapStyle( mLineCapStyle );
  mPen.setWidthF( 0.3 );

  mBrush.setColor( Qt::black );
  mBrush.setStyle( Qt::SolidPattern );

  mBrush2.setColor( Qt::white );
  mBrush2.setStyle( Qt::SolidPattern );

  //get default composer font from settings
  QSettings settings;
  QString defaultFontString = settings.value( QStringLiteral( "/Composer/defaultFont" ) ).toString();
  if ( !defaultFontString.isEmpty() )
  {
    mFont.setFamily( defaultFontString );
  }
  mFont.setPointSizeF( 12.0 );
  mFontColor = QColor( 0, 0, 0 );

  mLabelBarSpace = 3.0;
  mBoxContentSpace = 1.0;
  emit itemChanged();
}

void QgsComposerScaleBar::applyDefaultSize( QgsComposerScaleBar::ScaleBarUnits u )
{
  if ( mComposerMap )
  {
    setUnits( u );
    double upperMagnitudeMultiplier = 1.0;
    double widthInSelectedUnits = mapWidth();
    double initialUnitsPerSegment =  widthInSelectedUnits / 10.0; //default scalebar width equals half the map width
    setNumUnitsPerSegment( initialUnitsPerSegment );

    switch ( mUnits )
    {
      case MapUnits:
      {
        upperMagnitudeMultiplier = 1.0;
        setUnitLabeling( tr( "units" ) );
        break;
      }
      case Meters:
      {
        if ( initialUnitsPerSegment > 1000.0 )
        {
          upperMagnitudeMultiplier = 1000.0;
          setUnitLabeling( tr( "km" ) );
        }
        else
        {
          upperMagnitudeMultiplier = 1.0;
          setUnitLabeling( tr( "m" ) );
        }
        break;
      }
      case Feet:
      {
        if ( initialUnitsPerSegment > 5419.95 )
        {
          upperMagnitudeMultiplier = 5419.95;
          setUnitLabeling( tr( "miles" ) );
        }
        else
        {
          upperMagnitudeMultiplier = 1.0;
          setUnitLabeling( tr( "ft" ) );
        }
        break;
      }
      case NauticalMiles:
      {
        upperMagnitudeMultiplier = 1;
        setUnitLabeling( tr( "Nm" ) );
        break;
      }
    }

    double segmentWidth = initialUnitsPerSegment / upperMagnitudeMultiplier;
    int segmentMagnitude = floor( log10( segmentWidth ) );
    double unitsPerSegment = upperMagnitudeMultiplier * ( qPow( 10.0, segmentMagnitude ) );
    double multiplier = floor(( widthInSelectedUnits / ( unitsPerSegment * 10.0 ) ) / 2.5 ) * 2.5;

    if ( multiplier > 0 )
    {
      unitsPerSegment = unitsPerSegment * multiplier;
    }
    setNumUnitsPerSegment( unitsPerSegment );
    setNumMapUnitsPerScaleBarUnit( upperMagnitudeMultiplier );

    setNumSegments( 4 );
    setNumSegmentsLeft( 2 );
  }

  refreshSegmentMillimeters();
  adjustBoxSize();
  emit itemChanged();
}

void QgsComposerScaleBar::adjustBoxSize()
{
  if ( !mStyle )
  {
    return;
  }

  QRectF box = mStyle->calculateBoxSize();
  if ( rect().height() > box.height() )
  {
    //keep user specified item height if higher than minimum scale bar height
    box.setHeight( rect().height() );
  }

  //update rect for data defined size and position
  QRectF newRect = evalItemRect( box, true );

  //scale bars have a minimum size, respect that regardless of data defined settings
  if ( newRect.width() < box.width() )
  {
    newRect.setWidth( box.width() );
  }
  if ( newRect.height() < box.height() )
  {
    newRect.setHeight( box.height() );
  }

  QgsComposerItem::setSceneRect( newRect );
}

void QgsComposerScaleBar::setSceneRect( const QRectF& rectangle )
{
  QRectF box = mStyle->calculateBoxSize();
  if ( rectangle.height() > box.height() )
  {
    //keep user specified item height if higher than minimum scale bar height
    box.setHeight( rectangle.height() );
  }
  box.moveTopLeft( rectangle.topLeft() );

  //update rect for data defined size and position
  QRectF newRect = evalItemRect( rectangle );

  //scale bars have a minimum size, respect that regardless of data defined settings
  if ( newRect.width() < box.width() )
  {
    newRect.setWidth( box.width() );
  }
  if ( newRect.height() < box.height() )
  {
    newRect.setHeight( box.height() );
  }

  QgsComposerItem::setSceneRect( newRect );
}

void QgsComposerScaleBar::update()
{
  //Don't adjust box size for numeric scale bars:
  if ( mStyle && mStyle->name() != QLatin1String( "Numeric" ) )
  {
    adjustBoxSize();
  }
  QgsComposerItem::update();
}

void QgsComposerScaleBar::updateSegmentSize()
{
  if ( !mStyle )
  {
    return;
  }
  double width = mStyle->calculateBoxSize().width();
  refreshSegmentMillimeters();
  double widthAfter = mStyle->calculateBoxSize().width();
  correctXPositionAlignment( width, widthAfter );
  update();
  emit itemChanged();
}

void QgsComposerScaleBar::segmentPositions( QList<QPair<double, double> >& posWidthList ) const
{
  posWidthList.clear();
  double mCurrentXCoord = mPen.widthF() + mBoxContentSpace;

  //left segments
  double leftSegmentSize = mSegmentMillimeters / mNumSegmentsLeft;
  for ( int i = 0; i < mNumSegmentsLeft; ++i )
  {
    posWidthList.push_back( qMakePair( mCurrentXCoord, leftSegmentSize ) );
    mCurrentXCoord += leftSegmentSize;
  }

  //right segments
  for ( int i = 0; i < mNumSegments; ++i )
  {
    posWidthList.push_back( qMakePair( mCurrentXCoord, mSegmentMillimeters ) );
    mCurrentXCoord += mSegmentMillimeters;
  }
}

void QgsComposerScaleBar::setStyle( const QString& styleName )
{
  delete mStyle;
  mStyle = nullptr;

  //switch depending on style name
  if ( styleName == QLatin1String( "Single Box" ) )
  {
    mStyle = new QgsSingleBoxScaleBarStyle( this );
  }
  else if ( styleName == QLatin1String( "Double Box" ) )
  {
    mStyle = new QgsDoubleBoxScaleBarStyle( this );
  }
  else if ( styleName == QLatin1String( "Line Ticks Middle" )  || styleName == QLatin1String( "Line Ticks Down" ) || styleName == QLatin1String( "Line Ticks Up" ) )
  {
    QgsTicksScaleBarStyle* tickStyle = new QgsTicksScaleBarStyle( this );
    if ( styleName == QLatin1String( "Line Ticks Middle" ) )
    {
      tickStyle->setTickPosition( QgsTicksScaleBarStyle::TicksMiddle );
    }
    else if ( styleName == QLatin1String( "Line Ticks Down" ) )
    {
      tickStyle->setTickPosition( QgsTicksScaleBarStyle::TicksDown );
    }
    else if ( styleName == QLatin1String( "Line Ticks Up" ) )
    {
      tickStyle->setTickPosition( QgsTicksScaleBarStyle::TicksUp );
    }
    mStyle = tickStyle;
  }
  else if ( styleName == QLatin1String( "Numeric" ) )
  {
    mStyle = new QgsNumericScaleBarStyle( this );
  }
  emit itemChanged();
}

QString QgsComposerScaleBar::style() const
{
  if ( mStyle )
  {
    return mStyle->name();
  }
  else
  {
    return QLatin1String( "" );
  }
}

QString QgsComposerScaleBar::firstLabelString() const
{
  if ( mNumSegmentsLeft > 0 )
  {
    return QString::number( mNumUnitsPerSegment / mNumMapUnitsPerScaleBarUnit );
  }
  else
  {
    return QStringLiteral( "0" );
  }
}

QFont QgsComposerScaleBar::font() const
{
  return mFont;
}

void QgsComposerScaleBar::setFont( const QFont& font )
{
  mFont = font;
  update();
  emit itemChanged();
}

bool QgsComposerScaleBar::writeXml( QDomElement& elem, QDomDocument & doc ) const
{
  if ( elem.isNull() )
  {
    return false;
  }

  QDomElement composerScaleBarElem = doc.createElement( QStringLiteral( "ComposerScaleBar" ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "height" ), QString::number( mHeight ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "labelBarSpace" ), QString::number( mLabelBarSpace ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "boxContentSpace" ), QString::number( mBoxContentSpace ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "numSegments" ), mNumSegments );
  composerScaleBarElem.setAttribute( QStringLiteral( "numSegmentsLeft" ), mNumSegmentsLeft );
  composerScaleBarElem.setAttribute( QStringLiteral( "numUnitsPerSegment" ), QString::number( mNumUnitsPerSegment ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "segmentSizeMode" ), mSegmentSizeMode );
  composerScaleBarElem.setAttribute( QStringLiteral( "minBarWidth" ), mMinBarWidth );
  composerScaleBarElem.setAttribute( QStringLiteral( "maxBarWidth" ), mMaxBarWidth );
  composerScaleBarElem.setAttribute( QStringLiteral( "segmentMillimeters" ), QString::number( mSegmentMillimeters ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "numMapUnitsPerScaleBarUnit" ), QString::number( mNumMapUnitsPerScaleBarUnit ) );
  composerScaleBarElem.appendChild( QgsFontUtils::toXmlElement( mFont, doc, QStringLiteral( "scaleBarFont" ) ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "outlineWidth" ), QString::number( mPen.widthF() ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "unitLabel" ), mUnitLabeling );
  composerScaleBarElem.setAttribute( QStringLiteral( "units" ), mUnits );
  composerScaleBarElem.setAttribute( QStringLiteral( "lineJoinStyle" ), QgsSymbolLayerUtils::encodePenJoinStyle( mLineJoinStyle ) );
  composerScaleBarElem.setAttribute( QStringLiteral( "lineCapStyle" ), QgsSymbolLayerUtils::encodePenCapStyle( mLineCapStyle ) );

  //style
  if ( mStyle )
  {
    composerScaleBarElem.setAttribute( QStringLiteral( "style" ), mStyle->name() );
  }

  //map id
  if ( mComposerMap )
  {
    composerScaleBarElem.setAttribute( QStringLiteral( "mapId" ), mComposerMap->id() );
  }

  //colors

  //fill color
  QDomElement fillColorElem = doc.createElement( QStringLiteral( "fillColor" ) );
  QColor fillColor = mBrush.color();
  fillColorElem.setAttribute( QStringLiteral( "red" ), QString::number( fillColor.red() ) );
  fillColorElem.setAttribute( QStringLiteral( "green" ), QString::number( fillColor.green() ) );
  fillColorElem.setAttribute( QStringLiteral( "blue" ), QString::number( fillColor.blue() ) );
  fillColorElem.setAttribute( QStringLiteral( "alpha" ), QString::number( fillColor.alpha() ) );
  composerScaleBarElem.appendChild( fillColorElem );

  //fill color 2
  QDomElement fillColor2Elem = doc.createElement( QStringLiteral( "fillColor2" ) );
  QColor fillColor2 = mBrush2.color();
  fillColor2Elem.setAttribute( QStringLiteral( "red" ), QString::number( fillColor2.red() ) );
  fillColor2Elem.setAttribute( QStringLiteral( "green" ), QString::number( fillColor2.green() ) );
  fillColor2Elem.setAttribute( QStringLiteral( "blue" ), QString::number( fillColor2.blue() ) );
  fillColor2Elem.setAttribute( QStringLiteral( "alpha" ), QString::number( fillColor2.alpha() ) );
  composerScaleBarElem.appendChild( fillColor2Elem );

  //pen color
  QDomElement strokeColorElem = doc.createElement( QStringLiteral( "strokeColor" ) );
  QColor strokeColor = mPen.color();
  strokeColorElem.setAttribute( QStringLiteral( "red" ), QString::number( strokeColor.red() ) );
  strokeColorElem.setAttribute( QStringLiteral( "green" ), QString::number( strokeColor.green() ) );
  strokeColorElem.setAttribute( QStringLiteral( "blue" ), QString::number( strokeColor.blue() ) );
  strokeColorElem.setAttribute( QStringLiteral( "alpha" ), QString::number( strokeColor.alpha() ) );
  composerScaleBarElem.appendChild( strokeColorElem );

  //font color
  QDomElement fontColorElem = doc.createElement( QStringLiteral( "textColor" ) );
  fontColorElem.setAttribute( QStringLiteral( "red" ), QString::number( mFontColor.red() ) );
  fontColorElem.setAttribute( QStringLiteral( "green" ), QString::number( mFontColor.green() ) );
  fontColorElem.setAttribute( QStringLiteral( "blue" ), QString::number( mFontColor.blue() ) );
  fontColorElem.setAttribute( QStringLiteral( "alpha" ), QString::number( mFontColor.alpha() ) );
  composerScaleBarElem.appendChild( fontColorElem );

  //alignment
  composerScaleBarElem.setAttribute( QStringLiteral( "alignment" ), QString::number( static_cast< int >( mAlignment ) ) );

  elem.appendChild( composerScaleBarElem );
  return _writeXml( composerScaleBarElem, doc );
}

bool QgsComposerScaleBar::readXml( const QDomElement& itemElem, const QDomDocument& doc )
{
  if ( itemElem.isNull() )
  {
    return false;
  }

  mHeight = itemElem.attribute( QStringLiteral( "height" ), QStringLiteral( "5.0" ) ).toDouble();
  mLabelBarSpace = itemElem.attribute( QStringLiteral( "labelBarSpace" ), QStringLiteral( "3.0" ) ).toDouble();
  mBoxContentSpace = itemElem.attribute( QStringLiteral( "boxContentSpace" ), QStringLiteral( "1.0" ) ).toDouble();
  mNumSegments = itemElem.attribute( QStringLiteral( "numSegments" ), QStringLiteral( "2" ) ).toInt();
  mNumSegmentsLeft = itemElem.attribute( QStringLiteral( "numSegmentsLeft" ), QStringLiteral( "0" ) ).toInt();
  mNumUnitsPerSegment = itemElem.attribute( QStringLiteral( "numUnitsPerSegment" ), QStringLiteral( "1.0" ) ).toDouble();
  mSegmentSizeMode = static_cast<SegmentSizeMode>( itemElem.attribute( QStringLiteral( "segmentSizeMode" ), QStringLiteral( "0" ) ).toInt() );
  mMinBarWidth = itemElem.attribute( QStringLiteral( "minBarWidth" ), QStringLiteral( "50" ) ).toInt();
  mMaxBarWidth = itemElem.attribute( QStringLiteral( "maxBarWidth" ), QStringLiteral( "150" ) ).toInt();
  mSegmentMillimeters = itemElem.attribute( QStringLiteral( "segmentMillimeters" ), QStringLiteral( "0.0" ) ).toDouble();
  mNumMapUnitsPerScaleBarUnit = itemElem.attribute( QStringLiteral( "numMapUnitsPerScaleBarUnit" ), QStringLiteral( "1.0" ) ).toDouble();
  mPen.setWidthF( itemElem.attribute( QStringLiteral( "outlineWidth" ), QStringLiteral( "0.3" ) ).toDouble() );
  mUnitLabeling = itemElem.attribute( QStringLiteral( "unitLabel" ) );
  mLineJoinStyle = QgsSymbolLayerUtils::decodePenJoinStyle( itemElem.attribute( QStringLiteral( "lineJoinStyle" ), QStringLiteral( "miter" ) ) );
  mPen.setJoinStyle( mLineJoinStyle );
  mLineCapStyle = QgsSymbolLayerUtils::decodePenCapStyle( itemElem.attribute( QStringLiteral( "lineCapStyle" ), QStringLiteral( "square" ) ) );
  mPen.setCapStyle( mLineCapStyle );
  if ( !QgsFontUtils::setFromXmlChildNode( mFont, itemElem, QStringLiteral( "scaleBarFont" ) ) )
  {
    mFont.fromString( itemElem.attribute( QStringLiteral( "font" ), QLatin1String( "" ) ) );
  }

  //colors
  //fill color
  QDomNodeList fillColorList = itemElem.elementsByTagName( QStringLiteral( "fillColor" ) );
  if ( !fillColorList.isEmpty() )
  {
    QDomElement fillColorElem = fillColorList.at( 0 ).toElement();
    bool redOk, greenOk, blueOk, alphaOk;
    int fillRed, fillGreen, fillBlue, fillAlpha;

    fillRed = fillColorElem.attribute( QStringLiteral( "red" ) ).toDouble( &redOk );
    fillGreen = fillColorElem.attribute( QStringLiteral( "green" ) ).toDouble( &greenOk );
    fillBlue = fillColorElem.attribute( QStringLiteral( "blue" ) ).toDouble( &blueOk );
    fillAlpha = fillColorElem.attribute( QStringLiteral( "alpha" ) ).toDouble( &alphaOk );

    if ( redOk && greenOk && blueOk && alphaOk )
    {
      mBrush.setColor( QColor( fillRed, fillGreen, fillBlue, fillAlpha ) );
    }
  }
  else
  {
    mBrush.setColor( QColor( itemElem.attribute( QStringLiteral( "brushColor" ), QStringLiteral( "#000000" ) ) ) );
  }

  //fill color 2
  QDomNodeList fillColor2List = itemElem.elementsByTagName( QStringLiteral( "fillColor2" ) );
  if ( !fillColor2List.isEmpty() )
  {
    QDomElement fillColor2Elem = fillColor2List.at( 0 ).toElement();
    bool redOk, greenOk, blueOk, alphaOk;
    int fillRed, fillGreen, fillBlue, fillAlpha;

    fillRed = fillColor2Elem.attribute( QStringLiteral( "red" ) ).toDouble( &redOk );
    fillGreen = fillColor2Elem.attribute( QStringLiteral( "green" ) ).toDouble( &greenOk );
    fillBlue = fillColor2Elem.attribute( QStringLiteral( "blue" ) ).toDouble( &blueOk );
    fillAlpha = fillColor2Elem.attribute( QStringLiteral( "alpha" ) ).toDouble( &alphaOk );

    if ( redOk && greenOk && blueOk && alphaOk )
    {
      mBrush2.setColor( QColor( fillRed, fillGreen, fillBlue, fillAlpha ) );
    }
  }
  else
  {
    mBrush2.setColor( QColor( itemElem.attribute( QStringLiteral( "brush2Color" ), QStringLiteral( "#ffffff" ) ) ) );
  }

  //stroke color
  QDomNodeList strokeColorList = itemElem.elementsByTagName( QStringLiteral( "strokeColor" ) );
  if ( !strokeColorList.isEmpty() )
  {
    QDomElement strokeColorElem = strokeColorList.at( 0 ).toElement();
    bool redOk, greenOk, blueOk, alphaOk;
    int strokeRed, strokeGreen, strokeBlue, strokeAlpha;

    strokeRed = strokeColorElem.attribute( QStringLiteral( "red" ) ).toDouble( &redOk );
    strokeGreen = strokeColorElem.attribute( QStringLiteral( "green" ) ).toDouble( &greenOk );
    strokeBlue = strokeColorElem.attribute( QStringLiteral( "blue" ) ).toDouble( &blueOk );
    strokeAlpha = strokeColorElem.attribute( QStringLiteral( "alpha" ) ).toDouble( &alphaOk );

    if ( redOk && greenOk && blueOk && alphaOk )
    {
      mPen.setColor( QColor( strokeRed, strokeGreen, strokeBlue, strokeAlpha ) );
    }
  }
  else
  {
    mPen.setColor( QColor( itemElem.attribute( QStringLiteral( "penColor" ), QStringLiteral( "#000000" ) ) ) );
  }

  //font color
  QDomNodeList textColorList = itemElem.elementsByTagName( QStringLiteral( "textColor" ) );
  if ( !textColorList.isEmpty() )
  {
    QDomElement textColorElem = textColorList.at( 0 ).toElement();
    bool redOk, greenOk, blueOk, alphaOk;
    int textRed, textGreen, textBlue, textAlpha;

    textRed = textColorElem.attribute( QStringLiteral( "red" ) ).toDouble( &redOk );
    textGreen = textColorElem.attribute( QStringLiteral( "green" ) ).toDouble( &greenOk );
    textBlue = textColorElem.attribute( QStringLiteral( "blue" ) ).toDouble( &blueOk );
    textAlpha = textColorElem.attribute( QStringLiteral( "alpha" ) ).toDouble( &alphaOk );

    if ( redOk && greenOk && blueOk && alphaOk )
    {
      mFontColor = QColor( textRed, textGreen, textBlue, textAlpha );
    }
  }
  else
  {
    mFontColor.setNamedColor( itemElem.attribute( QStringLiteral( "fontColor" ), QStringLiteral( "#000000" ) ) );
  }

  //style
  delete mStyle;
  mStyle = nullptr;
  QString styleString = itemElem.attribute( QStringLiteral( "style" ), QLatin1String( "" ) );
  setStyle( tr( styleString.toLocal8Bit().data() ) );

  mUnits = static_cast< ScaleBarUnits >( itemElem.attribute( QStringLiteral( "units" ) ).toInt() );
  mAlignment = static_cast< Alignment >( itemElem.attribute( QStringLiteral( "alignment" ), QStringLiteral( "0" ) ).toInt() );

  //map
  int mapId = itemElem.attribute( QStringLiteral( "mapId" ), QStringLiteral( "-1" ) ).toInt();
  if ( mapId >= 0 )
  {
    const QgsComposerMap* composerMap = mComposition->getComposerMapById( mapId );
    mComposerMap = composerMap;
    if ( mComposerMap )
    {
      connect( mComposerMap, SIGNAL( extentChanged() ), this, SLOT( updateSegmentSize() ) );
      connect( mComposerMap, SIGNAL( destroyed( QObject* ) ), this, SLOT( invalidateCurrentMap() ) );
    }
  }

  updateSegmentSize();

  //restore general composer item properties
  QDomNodeList composerItemList = itemElem.elementsByTagName( QStringLiteral( "ComposerItem" ) );
  if ( !composerItemList.isEmpty() )
  {
    QDomElement composerItemElem = composerItemList.at( 0 ).toElement();
    _readXml( composerItemElem, doc );
  }

  return true;
}

void QgsComposerScaleBar::correctXPositionAlignment( double width, double widthAfter )
{
  //Don't adjust position for numeric scale bars:
  if ( mStyle->name() == QLatin1String( "Numeric" ) )
  {
    return;
  }

  if ( mAlignment == Middle )
  {
    move( -( widthAfter - width ) / 2.0, 0 );
  }
  else if ( mAlignment == Right )
  {
    move( -( widthAfter - width ), 0 );
  }
}

