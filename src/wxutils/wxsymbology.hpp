#ifndef WXSYMBOLOGY_HPP
#define WXSYMBOLOGY_HPP

// An array of colours and corresponding bitmaps used to select
// colours for layers etc.

#include <string>

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

    [[nodiscard]] int Size() const;
    int AddColour( const wxColour &colour );
    [[nodiscard]] int FindColour( const wxColour &colour ) const;
    [[nodiscard]] const wxColour &Colour( int i ) const;
    [[nodiscard]] const wxBitmap &Bitmap( int i ) const;

    [[nodiscard]] int PaletteBitmapSize() const;

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
    SymbologyBase( const wxString& name );
    virtual ~SymbologyBase();
    wxString Name() { return name; }
    [[nodiscard]] const std::string &NameString() const { return cname; }
    wxString Identifier() { return id; }
private:
    wxString name;
    std::string cname;
    wxString id;
};

class SymbologyList
{
public:
    SymbologyList();
    ~SymbologyList();
    int AddSymbology( SymbologyBase *symbology );
    [[nodiscard]] int Size() const;
    [[nodiscard]] SymbologyBase *GetSymbology( int i ) const;
    [[nodiscard]] SymbologyBase *GetSymbology( wxString name ) const;
    [[nodiscard]] int GetSymbologyId( const wxString& name ) const;
    [[nodiscard]] int GetSymbologyByIdentifier( const wxString& uid ) const;
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

    LayerSymbology( wxString name, int type, int colourId, bool show, bool isControlCheckbox = false, bool colourEditable = true );
    [[nodiscard]] int Type() const;
    [[nodiscard]] int ColourId() const;
    void SetColourId( int newColourId );
    [[nodiscard]] bool Show() const;
    [[nodiscard]] bool Status() const;
    void SetStatus( bool newStatus );
    [[nodiscard]] bool HasColour() const;
    [[nodiscard]] bool HasStatus() const;
    [[nodiscard]] bool IsControlCheckbox() const;
    [[nodiscard]] bool IsMixedRowStatus() const;
    void SetMixedRowStatus( bool newMixedRowStatus );
    // Whether the swatch should currently be drawn/clickable. Independent of
    // HasColour(): a row can have a colour value that persists across
    // rebuilds while its swatch is temporarily hidden because it isn't the
    // list currently selected for Colour by.
    [[nodiscard]] bool ColourEditable() const;
private:
    int type;
    int colourId;
    bool show;
    // true if this row's checkbox controls a following range of
    // other rows' status, instead of its own
    bool isControlCheckbox;
    // when isControlCheckbox, true forces this row's checkbox glyph
    // indeterminate, overriding the checked/unchecked mark show would
    // otherwise draw; else unused
    bool mixedRowStatus;
    bool colourEditable;
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
    ~PointSymbology() override;

    void SetSymbol( int nNodes = 0, double size = 1.0, double indent = 0.0, const wxColour &colourBorder = wxNullColour, const wxColour &colourFill = wxNullColour  );
    void AddSymbol( int nNodes, double size = 1.0, double indent = 0.0, const wxColour &colourBorder = wxNullColour, const wxColour &colourFill = wxNullColour  );
    void Render( wxDC &dc, const wxPoint &pt, int refSize, const wxColour &defaultColour );
    [[nodiscard]] int NPoints() const { return npt; }
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
    void DrawText( wxDC &dc, int refSize, wxPoint refPoint, const wxString& string ) const;
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
    ~TextStyle() override;

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
    void InitialisePalette( const ColourPalette &basepalette );

    int AddLayer( wxString name, int type, const wxColour &colour, bool display = true, bool isControlCheckbox = false, bool colourEditable = true );
    void AddTitle(wxString name);
    void AddSpacer();
    [[nodiscard]] int LayerCount() const;
    [[nodiscard]] int GetLayerByName( wxString name ) const;
    [[nodiscard]] int GetLayerByIdentifier( wxString uid ) const;
    [[nodiscard]] LayerSymbology &GetLayer( int i ) const;
    [[nodiscard]] const wxColour &LayerColour( int i ) const;
    [[nodiscard]] const wxBitmap &LayerBitmap( int i ) const;
    // Note: Symbology takes over ownership of PointSymbology
    int AddPointSymbol( PointSymbology *ptsym );
    [[nodiscard]] int GetPointSymbolId( wxString name ) const;
    [[nodiscard]] PointSymbology *GetPointSymbol( int i ) const;
    // TextStyle ... basically just a wrapper around wxFont...
    int AddTextStyle( TextStyle *txtsym );
    [[nodiscard]] int GetTextStyleId( wxString name ) const;
    [[nodiscard]] TextStyle *GetTextStyle( int i ) const;

    [[nodiscard]] bool ShowLayer( int i ) const;
    [[nodiscard]] int PaletteBitmapSize() const;

private:
    ColourPalette palette;
    SymbologyList layers;
    SymbologyList pointSymbols;
    SymbologyList textStyles;
};

#endif
