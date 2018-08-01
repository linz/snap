#ifndef WXSYMBOLOGY_HPP
#define WXSYMBOLOGY_HPP

// An array of colours and corresponding bitmaps used to select
// colours for layers etc.

#include "wx_includes.hpp"

class ColourPalette
{

private:
    class Item
    {
    public:
        Item( ColourPalette *palette, const wxColour &colour );
        ~Item();
        const wxColour &Colour() { return colour; }
        const wxBitmap &Bitmap();

    private:
        ColourPalette *palette;
        wxColour colour;
        wxBitmap *bitmap;

    private:
        void CreateBitmap();

        DECLARE_NO_COPY_CLASS(Item)
    };

public:
    ColourPalette();
    ~ColourPalette();

    int Size() const;
    int AddColour( const wxColour &colour );
    int FindColour( const wxColour &colour ) const;
    const wxColour &Colour( int i ) const;
    const wxBitmap &Bitmap( int i ) const;

    int PaletteBitmapSize() const;

private:
    void Grow();

    static int DefaultSize;

    ColourPalette::Item **palette;
    int maxsize;
    int size;

    int paletteBitmapSize;
};

// Symbology base class and symbology list, manage referencing symbologies by
// name and index.  Basically just an expandable array..

class SymbologyBase
{
public:
    SymbologyBase( wxString name );
    virtual ~SymbologyBase();
    wxString Name() { return name; }
    const char *NamePtr(){ return cname; }
    wxString Identifier() { return id; }
private:
    wxString name;
    char *cname;
    wxString id;
};

class SymbologyList
{
public:
    SymbologyList();
    ~SymbologyList();
    int AddSymbology( SymbologyBase *symbology );
    int Size() const;
    SymbologyBase *GetSymbology( int i ) const;
    SymbologyBase *GetSymbology( wxString name ) const;
    int GetSymbologyId( wxString name ) const;
    int GetSymbologyByIdentifier( wxString uid ) const;
private:
    void Grow();
    int maxSize;
    int size;
    SymbologyBase **symbologies;
};

// Definition of a layer symbology to be used in the map
// Each symbology has name, an integer options value,
// a colour, and an on/off status.  The options define
// whether the colour and status are applicable.
// Symbologies with neither colour nor status are assumed
// to be headings for the symbology list.
//
// Colours are represented as an id in a palette


class LayerSymbology : public SymbologyBase
{
public:
    enum Type { label = 0, hasColour = 1, hasStatus = 2, hasColourAndStatus = 3 };

    LayerSymbology( wxString name, int type, int colourId, bool show );
    int Type() const;
    int ColourId() const;
    void SetColourId( int newColourId );
    bool Show() const;
    bool Status() const;
    void SetStatus( bool newStatus );
    bool HasColour() const;
    bool HasStatus() const;
private:
    int type;
    int colourId;
    bool show;
};


// Point symbologies

class PointSymbology : public SymbologyBase
{
public:
    // nNodes defines the number of nodes in the symbology.  negative is vertex at top.
    // positive is face at top. 0 is circle.
    // colourId references a palette colour
    // size is relative to a reference size on a DC, maybe based on converting a standard
    // font size to points.

    PointSymbology( wxString name, int nNodes = 0, double size = 1.0, double indent = 0.0, const wxColour &colourBorder = wxNullColour, const wxColour &colourFill = wxNullColour  );
    ~PointSymbology();

    void SetSymbol( int nNodes = 0, double size = 1.0, double indent = 0.0, const wxColour &colourBorder = wxNullColour, const wxColour &colourFill = wxNullColour  );
    void AddSymbol( int nNodes, double size = 1.0, double indent = 0.0, const wxColour &colourBorder = wxNullColour, const wxColour &colourFill = wxNullColour  );
    void Render( wxDC &dc, const wxPoint &pt, int refSize, const wxColour &defaultColour );
    int NPoints() { return npt; }
    bool GetPoint( int ipt, double *x, double *y );

private:
    void CalcOffsets( int refSize );

    int nNode;
    wxColour penColour;
    wxColour fillColour;

    int npt;
    double size;
    double size2;
    double cosStart;
    double sinStart;
    double cosAngle;
    double sinAngle;

    wxPoint *pointOffsets;

    int refSize;
    PointSymbology *next;
};

// TextAlign defines alignment of text relative to a point

class TextAlign
{
public:
    // offsetX -1 to 1 run from right to left justified, <-1 and >1 offset by specified number of
    // refSize units.  Similarly for offsetY
    TextAlign() { SetAlignment( 0, 0 ); }
    TextAlign( float alignX, float alignY ) { SetAlignment( alignX, alignY ); }
    // dc should be set with the appropriate font,
    void SetAlignment( float alignX, float alignY );
    void DrawText( wxDC &dc, int refSize, wxPoint refPoint, wxString string ) const;
private:
    float textX;
    float textY;
    float paddingX;
    float paddingY;
};

// TextStyle: Combines a font and colour

class TextStyle : public SymbologyBase
{
public:
    TextStyle( wxString name, const wxFont &font, const wxColour &colour = wxNullColour );
    ~TextStyle();

    void SetFont( const wxFont &font, const wxColour &colour = wxNullColour );
    void Render( wxDC &dc, wxPoint &pt, wxString string, const TextAlign &alignment, int refSize, const wxColour &defaultColour );
private:
    wxFont font;
    int baseSize;
    int fontRefSize;
    wxColour colour;
};

// List of symbologies forming a key
// Note: takes ownership of all symbologies added with AddLayer, AddPointSymbology, ...

class Symbology
{
public:
    Symbology();
    ~Symbology();

    ColourPalette *GetPalette() { return &palette ; }
    void InitiallizePalette( const ColourPalette &basepalette );

    int AddLayer( wxString name, int type, const wxColour &colour, bool display = true );
    void AddTitle(wxString name);
    void AddSpacer();
    int LayerCount() const;
    int GetLayerByName( wxString name ) const;
    int GetLayerByIdentifier( wxString uid ) const;
    LayerSymbology &GetLayer( int i ) const;
    const wxColour &LayerColour( int i ) const;
    const wxBitmap &LayerBitmap( int i ) const;
    // Note: Symbology takes over ownership of PointSymbology
    int AddPointSymbol( PointSymbology *ptsym );
    int GetPointSymbolId( wxString name ) const;
    PointSymbology *GetPointSymbol( int i ) const;
    // TextStyle ... basically just a wrapper around wxFont...
    int AddTextStyle( TextStyle *txtsym );
    int GetTextStyleId( wxString name ) const;
    TextStyle *GetTextStyle( int i ) const;

    bool ShowLayer( int i ) const;
    int PaletteBitmapSize() const;

private:
    ColourPalette palette;
    SymbologyList layers;
    SymbologyList pointSymbols;
    SymbologyList textStyles;
};

#endif
