#include "snapconfig.h"
// Classes for managing the symbology in the SNAPPLOT application.

#include "wxsymbology.hpp"
#include <string.h>
#include <stdlib.h>

// ColourPalette::Item: A class holding a colour definition and a corresponding
// bitmap for a ColourPalette.

ColourPalette::Item::Item( ColourPalette *palette, const wxColour &colour ) :
    palette(palette),
    colour(colour),
    bitmap(0)
{
}

ColourPalette::Item::~Item()
{
    if( bitmap ) delete bitmap;
}

const wxBitmap &ColourPalette::Item::Bitmap()
{
    if( ! bitmap ) CreateBitmap();
    return *bitmap;
}

void ColourPalette::Item::CreateBitmap()
{
    if( bitmap ) return;

    int size = palette->PaletteBitmapSize();
    bitmap = new wxBitmap( size, size );
    wxMemoryDC dc;
    dc.SelectObject( *bitmap );
    dc.SetBrush( Colour());
    dc.SetPen( *wxWHITE );
    dc.DrawRectangle( 0, 0, size, size );
    dc.SetPen( wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW) );
    dc.DrawLine(0,size-2,0,0);
    dc.DrawLine(0,0,size-1,0);
    dc.SetPen( wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW) );
    dc.DrawLine(1,size-3,1,1);
    dc.DrawLine(1,1,size-2,1);
    dc.SetPen( wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE) );
    dc.DrawLine( 1, size-2, size-2, size-2 );
    dc.DrawLine( size-2, size-2, size-2, 0 );
}

// ColourPalette class
// Used for managing a palette of colours used in the drawing.
//

int ColourPalette::DefaultSize = 32;

ColourPalette::ColourPalette()
{
    // Create an array of Item pointers

    maxsize = DefaultSize;
    if( maxsize < 32 ) maxsize = 32;
    palette = new ColourPalette::Item *[maxsize];
    size = 0;

    // Default bitmap settings..

    paletteBitmapSize = 13;
}

ColourPalette::~ColourPalette()
{
    for( int nitem = 0; nitem < size; nitem++ )
    {
        delete palette[nitem];
        palette[nitem] = 0;
    }
    delete [] palette;
}

int ColourPalette::Size() const
{
    return size;
}

int ColourPalette::FindColour( const wxColour &colour ) const
{
    for( int nitem = 0; nitem < size; nitem++ )
    {
        if( palette[nitem]->Colour() == colour ) return nitem;
    }
    return -1;
}

int ColourPalette::AddColour( const wxColour &colour )
{
    int ncolour = FindColour( colour );
    if( ncolour >= 0 ) return ncolour;

    ncolour = size;
    if( ncolour >= maxsize ) Grow();
    palette[ncolour] = new Item( this, colour );
    size++;

    return ncolour;
}

void ColourPalette::Grow()
{
    ColourPalette::Item **newpalette = new ColourPalette::Item* [maxsize*2];
    maxsize *= 2;
    for( int i = 0; i < size; i++ ) { newpalette[i] = palette[i]; }
    delete [] palette;
    palette = newpalette;
}

const wxColour & ColourPalette::Colour(int i) const
{
    wxASSERT( i >= 0 && i < size );
    return palette[i]->Colour();
}
const wxBitmap & ColourPalette::Bitmap(int i) const
{
    wxASSERT( i >= 0 && i < size );
    return palette[i]->Bitmap();
}

int ColourPalette::PaletteBitmapSize() const
{
    return paletteBitmapSize;
}

// SymbologyBase ... nothing really in it!

SymbologyBase::SymbologyBase(wxString np)
{
    int i = np.Find('|');
    if( i == wxNOT_FOUND )
    {
        name = np;
        id = np;
    }
    else
    {
        id = np.Mid(0,i);
        name = np.Mid(i+1);
    }
    id.Replace(" ","_");
    cname=strdup(name.mb_str());
}

SymbologyBase::~SymbologyBase()
{
    if( cname ) free(cname);
}

// SymbologyList ... an expandable array of symbologies.

SymbologyList::SymbologyList()
{
    maxSize = 16;
    symbologies = new SymbologyBase *[16];
    size = 0;
}

SymbologyList::~SymbologyList()
{
    if( symbologies )
    {
        for( int i = 0; i < size; i++ )
        {
            delete symbologies[i];
            symbologies[i] = 0;
        }
        delete [] symbologies;
    }
}

int SymbologyList::AddSymbology( SymbologyBase *symbology )
{
    // if( GetLayer(symbology->Name()) return;

    if( size >= maxSize ) Grow();
    symbologies[size] = symbology;
    size++;
    return size-1;
}

void SymbologyList::Grow()
{
    maxSize *= 2;
    SymbologyBase **newsymbologies = new SymbologyBase* [maxSize];
    for( int i = 0; i < size; i++ ) { newsymbologies[i] = symbologies[i]; }
    delete [] symbologies;
    symbologies = newsymbologies;
}

int SymbologyList::Size() const { return size; }

SymbologyBase *SymbologyList::GetSymbology( int i ) const
{
    if( i >= 0 && i < size ) return symbologies[i];
    return 0;
}

SymbologyBase *SymbologyList::GetSymbology( wxString name ) const
{
    return GetSymbology( GetSymbologyId( name ));
}

int SymbologyList::GetSymbologyId( wxString name ) const
{
    for( int i = 0; i < size; i++ )
    {
        if( symbologies[i]->Name() == name ) return i;
    }
    return -1;
}

int SymbologyList::GetSymbologyByIdentifier( wxString uid ) const
{
    for( int i = 0; i < size; i++ )
    {
        if( symbologies[i]->Identifier() == uid ) return i;
    }
    return -1;
}


// LayerSymbology. Simple class defining a symbology.

LayerSymbology::LayerSymbology(wxString name, int type, int colourId, bool show) :
    SymbologyBase(name),
    type(type),
    colourId(colourId),
    show(show)
{
}

int LayerSymbology::Type() const
{
    return type;
}

int LayerSymbology::ColourId() const
{
    return colourId;
}

void LayerSymbology::SetColourId( int newColourId )
{
    colourId = newColourId;
}

bool LayerSymbology::Show() const
{
    if( (type & hasStatus) == hasStatus )
    {
        return show;
    }
    else if( (type & hasColour) == hasColour )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool LayerSymbology::Status() const
{
    return show;
}

void LayerSymbology::SetStatus( bool newStatus )
{
    show = newStatus;
}

bool LayerSymbology::HasColour() const
{
    return (type & hasColour) == hasColour;
}

bool LayerSymbology::HasStatus() const
{
    return (type & hasStatus) == hasStatus;
}

// TextAlign: A class for rendering text at a specified offset relative to a point.
// Uses a DC preloaded with the font settings...

void TextAlign::SetAlignment( float alignX, float alignY )
{
    textX = alignX;
    textY = alignY;
    paddingX = 0.0;
    paddingY = 0.0;
    if( alignX > 1.0 )
    {
        textX = 1.0;
        paddingX = alignX - 1.0;
    }
    else if( alignX < -1.0 )
    {
        textX = -1.0;
        paddingX = alignX + 1.0;
    }
    if( alignY > 1.0 )
    {
        textY = 1.0;
        paddingY = alignY - 1.0;
    }
    else if( alignY < -1.0 )
    {
        textY = -1.0;
        paddingY = alignY + 1.0;
    }
    textX = (textX - 1)/2;
    textY = (textY - 1)/2;
}

void TextAlign::DrawText(wxDC &dc, int refSize, wxPoint refPoint, wxString string ) const
{
    int twidth;
    int theight;

    dc.GetTextExtent( string, &twidth, &theight );

    int dx = (int) (twidth*textX + refSize*paddingX);
    int dy = (int) (theight*textY + refSize*paddingY);

    dc.DrawText( string, refPoint.x + dx, refPoint.y + dy );
}

// TextStyle : Used to plot text

TextStyle::TextStyle(wxString name, const wxFont &font, const wxColour &colour ) :
    SymbologyBase(name)
{
    SetFont( font, colour );
}

TextStyle::~TextStyle()
{
}

void TextStyle::SetFont( const wxFont &font, const wxColour &colour )
{
    this->font = font;
    this->baseSize = font.GetPointSize();
    this->fontRefSize = 0;
    this->colour = colour;
}

void TextStyle::Render(wxDC &dc, wxPoint &pt, wxString string, const TextAlign &alignment, int refSize, const wxColour &defaultColour)
{
    if( refSize != fontRefSize )
    {
        int size = (baseSize * refSize)/10;
        font.SetPointSize(size);
        fontRefSize = refSize;
    }
    dc.SetFont( font );
    if( colour == wxNullColour )
    {
        dc.SetTextForeground( defaultColour );
    }
    else
    {
        dc.SetTextForeground( colour );
    }
    alignment.DrawText( dc, refSize,pt, string );
}

// PointSymbology: Used to plot points

// TODO: Use symmetry to avoid rounding errors making unsymmetric symbols

PointSymbology::PointSymbology( wxString name, int nNodes, double size, double indent, const wxColour &clrBorder, const wxColour &clrFill ) :
    SymbologyBase( name )
{
    pointOffsets = 0;
    next = 0;
    SetSymbol( nNodes, size, indent, clrBorder, clrFill );
}

PointSymbology::~PointSymbology()
{
    if( next ) delete next;
    if( pointOffsets ) delete [] pointOffsets;
}

void PointSymbology::SetSymbol( int nNodes, double size, double indent, const wxColour &clrBorder, const wxColour &clrFill )
{
    if( next ) delete next;
    next = 0;
    if( pointOffsets ) delete [] pointOffsets;
    pointOffsets = 0;

    if( nNodes < -10 || nNodes > 10 ) nNodes = 0;
    if( nNodes > -3 && nNodes < 3 ) nNodes = 0;
    penColour = clrBorder;
    fillColour = clrFill;
    refSize = 0;

    size /= 2;
    this->size = size;

    if( nNodes == 0 )
    {
        npt = 0;
    }
    else
    {
        bool haveIndent = indent != 0.0;

        npt = nNodes > 0 ? nNodes : -nNodes;
        if( haveIndent ) npt += npt;

        double angle = (atan(1.0)*8.0)/npt;
        cosAngle = cos(angle);
        sinAngle = sin(angle);
        cosStart = 1.0;
        sinStart = 0.0;

        if( haveIndent )
        {
            size2 = size * cosAngle * (1 - indent);
            if( nNodes < 0 ) { cosStart = cosAngle; sinStart = sinAngle; }
        }
        else
        {
            size2 = size;
            if( nNodes < 0 ) { cosStart = cos(angle/2); sinStart = sin(angle/2); }
        }
        pointOffsets = new wxPoint[npt];
    }
}

void PointSymbology::AddSymbol(int nNodes, double size, double indent, const wxColour &colourBorder, const wxColour &colourFill)
{
    next = new PointSymbology("", nNodes, size, indent, colourBorder, colourFill );
}

void PointSymbology::CalcOffsets( int refSize )
{
    if( refSize == this->refSize ) return;
    this->refSize = refSize;
    if( npt == 0 ) return;

    double oy = -cosStart;
    double ox = sinStart;
    for( int i = 0; i < npt; i++ )
    {
        double s = i % 2 ? size : size2;
        pointOffsets[i].x = floor( ox * s * refSize + 0.5 );
        pointOffsets[i].y = floor( oy * s * refSize + 0.5 );
        s = ox * cosAngle + oy * sinAngle;
        oy = oy * cosAngle - ox * sinAngle;
        ox = s;
    }
}

bool PointSymbology::GetPoint( int ipt, double *x, double *y )
{
    if( npt == 0 && ipt == 0 )
    {
        *x = size;
        *y = 0.0;
        return false;
    }
    if( ipt < 0 || ipt > npt ) return false;
    CalcOffsets(1000.0);
    *x = pointOffsets[ipt].x/1000.0;
    *y = pointOffsets[ipt].y/1000.0;
    return true;
}


// TODO: Consider buffering of this symbology..

void PointSymbology::Render(wxDC &dc, const wxPoint &pt, int refSize, const wxColour &defaultColour)
{
    if( refSize <= 0 ) return;

    if( penColour == wxNullColour )
    {
        dc.SetPen( wxPen( defaultColour ) );
    }
    else
    {
        dc.SetPen( wxPen( penColour ) );
    }
    if( fillColour == wxNullColour )
    {
        dc.SetBrush( wxBrush( defaultColour ) );
    }
    else
    {
        dc.SetBrush( wxBrush( fillColour ) );
    }

    if( npt == 0 )
    {
        int radius = floor( size * refSize  + 0.5 );
        dc.DrawCircle( pt, radius );
    }
    else
    {
        if( refSize != this->refSize ) CalcOffsets( refSize );
        dc.DrawPolygon( npt, pointOffsets, pt.x, pt.y );
    }

    if( next ) next->Render( dc, pt, refSize, defaultColour );
}

// Symbology: A list of symbologies with an associated colour palette

Symbology::Symbology ()
{
}

Symbology::~Symbology()
{
}

void Symbology::InitiallizePalette( const ColourPalette &basepalette )
{
    int psize = basepalette.Size();

    for( int i = 0; i < psize; i++ )
    {
        palette.AddColour( basepalette.Colour( i ) );
    }
}

int Symbology::AddLayer(wxString name, int type, const wxColour &colour, bool display )
{
    int colourId = 0;
    if( colour.IsOk() ) colourId= palette.AddColour( colour );
    LayerSymbology *s = new LayerSymbology( name, type, colourId, display );
    return layers.AddSymbology( s );
}

void Symbology::AddTitle( wxString name )
{
    AddLayer( name, LayerSymbology::label, wxColour(0,0,0), true );
}

void Symbology::AddSpacer()
{
    AddTitle( "" );
}

int Symbology::LayerCount() const
{
    return layers.Size();
}

int Symbology::GetLayerByName( wxString name ) const
{
    return layers.GetSymbologyId( name );
}

int Symbology::GetLayerByIdentifier( wxString uid ) const
{
    return layers.GetSymbologyByIdentifier( uid );
}

LayerSymbology &Symbology::GetLayer( int i ) const
{
    return *(LayerSymbology *) layers.GetSymbology(i);
}


const wxColour &Symbology::LayerColour( int i ) const
{
    return palette.Colour( GetLayer(i).ColourId() );
}

const wxBitmap &Symbology::LayerBitmap( int i ) const
{
    return palette.Bitmap( GetLayer(i).ColourId() );
}

bool Symbology::ShowLayer( int i ) const
{
    return GetLayer(i).Show();
}

int Symbology::PaletteBitmapSize() const
{
    return palette.PaletteBitmapSize();
}


int Symbology::AddPointSymbol( PointSymbology *ptsym )
{
    return pointSymbols.AddSymbology( ptsym );
}

int Symbology::GetPointSymbolId( wxString name ) const
{
    return pointSymbols.GetSymbologyId( name );
}

PointSymbology *Symbology::GetPointSymbol( int i ) const
{
    return (PointSymbology *) pointSymbols.GetSymbology( i );
}

int Symbology::AddTextStyle( TextStyle *txtsym )
{
    return textStyles.AddSymbology( txtsym );
}

int Symbology::GetTextStyleId( wxString name ) const
{
    return textStyles.GetSymbologyId( name );
}


TextStyle *Symbology::GetTextStyle( int i ) const
{
    return (TextStyle *) textStyles.GetSymbology(i);
}

