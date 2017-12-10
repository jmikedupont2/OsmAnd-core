#include "VectorLine_P.h"
#include "VectorLine.h"
#include "Utilities.h"

#include "ignore_warnings_on_external_includes.h"
#include "restore_internal_warnings.h"

#include "MapSymbol.h"
#include "MapSymbolsGroup.h"
#include "VectorMapSymbol.h"
#include "OnSurfaceVectorMapSymbol.h"
#include "QKeyValueIterator.h"

OsmAnd::VectorLine_P::VectorLine_P(VectorLine* const owner_)
: _hasUnappliedChanges(false), _hasUnappliedPrimitiveChanges(false), _isHidden(false),
  _metersPerPixel(1.0), _mapZoomLevel(InvalidZoomLevel), _mapVisualZoom(0.f), _mapVisualZoomShift(0.f),
  owner(owner_)
{
}

OsmAnd::VectorLine_P::~VectorLine_P()
{
}

bool OsmAnd::VectorLine_P::isHidden() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _isHidden;
}

void OsmAnd::VectorLine_P::setIsHidden(const bool hidden)
{
    QWriteLocker scopedLocker(&_lock);
    
    _isHidden = hidden;
    _hasUnappliedChanges = true;
}

QVector<OsmAnd::PointI> OsmAnd::VectorLine_P::getPoints() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _points;
}

void OsmAnd::VectorLine_P::setPoints(const QVector<PointI>& points)
{
    QWriteLocker scopedLocker(&_lock);
    
    _points = points;
    _hasUnappliedChanges = true;
}

bool OsmAnd::VectorLine_P::hasUnappliedChanges() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _hasUnappliedChanges;
}

bool OsmAnd::VectorLine_P::hasUnappliedPrimitiveChanges() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _hasUnappliedPrimitiveChanges;
}

bool OsmAnd::VectorLine_P::isMapStateChanged(const MapState& mapState) const
{
    bool changed = qAbs(_mapZoomLevel + _mapVisualZoom - mapState.zoomLevel - mapState.visualZoom) > 0.5;
    //_mapVisualZoom != mapState.visualZoom ||
    //_mapVisualZoomShift != mapState.visualZoomShift;
    return changed;
}

void OsmAnd::VectorLine_P::applyMapState(const MapState& mapState)
{
    _metersPerPixel = mapState.metersPerPixel;
    _mapZoomLevel = mapState.zoomLevel;
    _mapVisualZoom = mapState.visualZoom;
    _mapVisualZoomShift = mapState.visualZoomShift;
}

bool OsmAnd::VectorLine_P::update(const MapState& mapState)
{
    QWriteLocker scopedLocker(&_lock);

    bool mapStateChanged = isMapStateChanged(mapState);
    if (mapStateChanged)
        applyMapState(mapState);

    _hasUnappliedPrimitiveChanges = mapStateChanged;
    return mapStateChanged;
}

bool OsmAnd::VectorLine_P::applyChanges()
{
    QReadLocker scopedLocker1(&_lock);
    
    if (!_hasUnappliedChanges && !_hasUnappliedPrimitiveChanges)
        return false;
    
    QReadLocker scopedLocker2(&_symbolsGroupsRegistryLock);
    for (const auto& symbolGroup_ : constOf(_symbolsGroupsRegistry))
    {
        const auto symbolGroup = symbolGroup_.lock();
        if (!symbolGroup)
            continue;
        
        for (const auto& symbol_ : constOf(symbolGroup->symbols))
        {
            symbol_->isHidden = _isHidden;
            
            if (const auto symbol = std::dynamic_pointer_cast<OnSurfaceVectorMapSymbol>(symbol_))
            {
                if (!_points.empty())
                {
                    symbol->setPosition31(_points[0]);
                    if (_hasUnappliedPrimitiveChanges && _points.size() > 1)
                    {
                        generatePrimitive(symbol);
                    }
                }
            }
        }
    }
    
    _hasUnappliedChanges = false;
    _hasUnappliedPrimitiveChanges = false;
    
    return true;
}

std::shared_ptr<OsmAnd::VectorLine::SymbolsGroup> OsmAnd::VectorLine_P::inflateSymbolsGroup() const
{
    QReadLocker scopedLocker(&_lock);
    
    // Construct new map symbols group for this marker
    const std::shared_ptr<VectorLine::SymbolsGroup> symbolsGroup(new VectorLine::SymbolsGroup(std::const_pointer_cast<VectorLine_P>(shared_from_this())));
    symbolsGroup->presentationMode |= MapSymbolsGroup::PresentationModeFlag::ShowAllOrNothing;
    
    if (_points.size() > 1)
    {
        const std::shared_ptr<OnSurfaceVectorMapSymbol> vectorLine(new OnSurfaceVectorMapSymbol(symbolsGroup));
        generatePrimitive(vectorLine);
        symbolsGroup->symbols.push_back(vectorLine);
        
    }
    
    return symbolsGroup;
}

std::shared_ptr<OsmAnd::VectorLine::SymbolsGroup> OsmAnd::VectorLine_P::createSymbolsGroup(const MapState& mapState)
{
    applyMapState(mapState);
    
    const auto inflatedSymbolsGroup = inflateSymbolsGroup();
    registerSymbolsGroup(inflatedSymbolsGroup);
    return inflatedSymbolsGroup;
}

void OsmAnd::VectorLine_P::registerSymbolsGroup(const std::shared_ptr<MapSymbolsGroup>& symbolsGroup) const
{
    QWriteLocker scopedLocker(&_symbolsGroupsRegistryLock);
    
    _symbolsGroupsRegistry.insert(symbolsGroup.get(), symbolsGroup);
}

void OsmAnd::VectorLine_P::unregisterSymbolsGroup(MapSymbolsGroup* const symbolsGroup) const
{
    QWriteLocker scopedLocker(&_symbolsGroupsRegistryLock);
    
    _symbolsGroupsRegistry.remove(symbolsGroup);
}

OsmAnd::PointD OsmAnd::VectorLine_P::findLineIntersection(PointD p1, OsmAnd::PointD p2, OsmAnd::PointD p3, OsmAnd::PointD p4) const
{
    double d = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
    if(d == 0) {
        // in case of lines connecting p2 == p3
        return p2;
    }
    OsmAnd::PointD r;
    r.x = ((p1.x* p2.y-p1.y*p2.x)*(p3.x - p4.x) - (p3.x* p4.y-p3.y*p4.x)*(p1.x - p2.x)) / d;
    r.y = ((p1.x* p2.y-p1.y*p2.x)*(p3.y - p4.y) - (p3.x* p4.y-p3.y*p4.x)*(p1.y - p2.y)) / d;
    
    return r;
}

std::shared_ptr<OsmAnd::OnSurfaceVectorMapSymbol> OsmAnd::VectorLine_P::generatePrimitive(const std::shared_ptr<OnSurfaceVectorMapSymbol> vectorLine) const
{
    vectorLine->releaseVerticesAndIndices();
    
    int order = owner->baseOrder;
    int pointsCount = _points.size();
    
    vectorLine->order = order++;
    vectorLine->position31 = _points[0];
    vectorLine->primitiveType = VectorMapSymbol::PrimitiveType::TriangleStrip;
    
    // Line has no reusable vertices - TODO clarify
    vectorLine->indices = nullptr;
    vectorLine->indicesCount = 0;
    
    vectorLine->scaleType = VectorMapSymbol::ScaleType::InMeters;
    vectorLine->scale = 1.0;
    vectorLine->direction = 0.f;
    
    vectorLine->verticesCount = (pointsCount - 2) * 4 + 2 * 2;
    vectorLine->vertices = new VectorMapSymbol::Vertex[vectorLine->verticesCount];
    
    
    double radius = owner->lineWidth * _metersPerPixel / 2;// * (qSqrt(_mapZoomLevel) / 3);
    
    auto pVertex = vectorLine->vertices;
    auto beginPoint = Utilities::convert31ToLatLon(PointI(_points[0].x, _points[0].y));
    
    std::vector<OsmAnd::PointD> b1(pointsCount), b2(pointsCount), e1(pointsCount), e2(pointsCount);
    double ntan = 0, nx1 = 0, ny1 = 0;
    for (auto pointIdx = 0u; pointIdx < pointsCount; pointIdx++)
    {
        
        double distX = Utilities::distance(
            Utilities::convert31ToLatLon(PointI(_points[pointIdx].x, _points[0].y)), beginPoint);
        double distY = Utilities::distance(
            Utilities::convert31ToLatLon(PointI(_points[0].x, _points[pointIdx].y)), beginPoint);
        if (_points[0].x > _points[pointIdx].x)
        {
            distX = -distX;
        }
        if (_points[0].y > _points[pointIdx].y)
        {
            distY = -distY;
        }
        b1[pointIdx] = OsmAnd::PointD(distX - nx1, distY + ny1);
        b2[pointIdx] = OsmAnd::PointD(distX + nx1, distY - ny1);
        if(pointIdx < pointsCount - 1) {
            ntan = atan2(_points[pointIdx+1].x - _points[pointIdx].x,
                            _points[pointIdx+1].y - _points[pointIdx].y);
            nx1 = radius * sin(M_PI_2 - ntan) ;
            ny1 = radius * cos(M_PI_2 - ntan) ;
        }
        
        e1[pointIdx] = OsmAnd::PointD(distX - nx1, distY + ny1);
        e2[pointIdx] = OsmAnd::PointD(distX + nx1, distY - ny1);
    }
    
    for (auto pointIdx = 0u; pointIdx < pointsCount; pointIdx++)
    {
        if (pointIdx == 0)
        {
            pVertex->positionXY[0] = e1[pointIdx].x;
            pVertex->positionXY[1] = e1[pointIdx].y;
            pVertex->color = owner->fillColor;
            pVertex += 1;
            
            pVertex->positionXY[0] = e2[pointIdx].x;
            pVertex->positionXY[1] = e2[pointIdx].y;
            pVertex->color = owner->fillColor;
            pVertex += 1;
        }
        else if (pointIdx == pointsCount - 1)
        {
            pVertex->positionXY[0] = b1[pointIdx].x;
            pVertex->positionXY[1] = b1[pointIdx].y;
            pVertex->color = owner->fillColor;
            pVertex += 1;
        
            pVertex->positionXY[0] = b2[pointIdx].x;
            pVertex->positionXY[1] = b2[pointIdx].y;
            pVertex->color = owner->fillColor;
            pVertex += 1;
        }
        else
        {
            PointD l1 = findLineIntersection(e1[pointIdx-1], b1[pointIdx], e1[pointIdx], b1[pointIdx+1]);
            PointD l2 = findLineIntersection(e2[pointIdx-1], b2[pointIdx], e1[pointIdx], b1[pointIdx+1]);
            PointD l3 = findLineIntersection(e1[pointIdx-1], b1[pointIdx], e2[pointIdx], b2[pointIdx+1]);
            PointD l4 = findLineIntersection(e2[pointIdx-1], b2[pointIdx], e2[pointIdx], b2[pointIdx+1]);
            //l1 = b1[pointIdx];
            l2 = b2[pointIdx];
            l3 = e1[pointIdx];
            //l4 = e2[pointIdx];
            // bewel - connecting only 3 points (one excluded depends on angle)
            // miter - connecting 4 points
            // round - generating between 2-3 point triangles (in place of triangle different between bewel/miter)
            
            pVertex->positionXY[0] = l1.x;
            pVertex->positionXY[1] = l1.y;
            pVertex->color = owner->fillColor;
            pVertex += 1;
            
            pVertex->positionXY[0] = l2.x;
            pVertex->positionXY[1] = l2.y;
            pVertex->color = owner->fillColor;
            pVertex += 1;
            
            pVertex->positionXY[0] = l3.x;
            pVertex->positionXY[1] = l3.y;
            pVertex->color = owner->fillColor;
            pVertex += 1;
            
            pVertex->positionXY[0] = l4.x;
            pVertex->positionXY[1] = l4.y;
            pVertex->color = owner->fillColor;
            pVertex += 1;
            
        }
    }
    
    vectorLine->isHidden = _isHidden;
    return vectorLine;
}