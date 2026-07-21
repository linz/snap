var wordindex=null;
var ready=false;
var pendingClickedLink=null;

function closeContentsLevel( item )
{
    item.addClass("closed");
}

function openContentsLevel( item )
{
    item.removeClass("closed");
}

function toggleContentsLevel( item )
{
    if( item.hasClass("closed"))
    {
        item.removeClass("closed");
    }
    else
    {
        item.addClass("closed");
    }
}

function helpRootRelativeUrl( absoluteUrl )
{
    let base=window.location.href.split(/[?#]/)[0];
    base=base.substring(0,base.lastIndexOf('/')+1);
    return absoluteUrl.startsWith(base) ? absoluteUrl.substring(base.length) : absoluteUrl;
}

function renderBreadcrumbs( levels )
{
    let bar=$("#breadcrumbs");
    bar.empty();
    levels.forEach(function(level,i){
        if( i > 0 ) bar.append($("<span>").addClass("breadcrumb-sep").text("/"));
        let item=level.children(".contents-item").first();
        let link=item.children("a").first();
        let label=item.text().trim();
        let isCurrent=(i == levels.length-1);
        if( link.length && ! isCurrent )
        {
            let href=link.attr("href");
            let crumb=$("<a>").addClass("breadcrumb-item").attr("href",href).text(label);
            crumb.click(function(event){
                event.preventDefault();
                event.stopPropagation();
                setPage(href,link);
                return false;
            });
            bar.append(crumb);
        }
        else
        {
            bar.append($("<span>").addClass("breadcrumb-item").text(label));
        }
    });
}

function highlightPage( url )
{
    // Some pages are linked from more than one place in the tree (eg the
    // alphabetic command list links to the same page under several command
    // names). pendingClickedLink - set by setPage() when the caller knows
    // exactly which sidebar/breadcrumb link was activated - disambiguates
    // those; navigation with no such origin (in-page content links, search
    // results, back/forward, initial load) falls back to the first match,
    // so highlighting and the breadcrumb trail always agree on one entry.
    $(".contents-item").removeClass("selected");
    $("#breadcrumbs").empty();
    let link=pendingClickedLink;
    pendingClickedLink=null;
    if( ! link || link.attr("href") != url )
    {
        link=$(".contents-item a").filter(function(){ return $(this).attr("href")==url; }).first();
    }
    if( link && link.length )
    {
        link.closest(".contents-item").addClass("selected");
        let levels=[];
        link.parents(".contents-level").each(function(){
            openContentsLevel($(this));
            levels.unshift($(this));
        });
        renderBreadcrumbs(levels);
    }
}

function setPage( url, link )
{
    pendingClickedLink=link || null;
    $('#help-page').attr("src",url);
}

function contentsItemRightEdge( item )
{
    let link=item.children("a").first();
    if( link.length ) return link.get(0).getBoundingClientRect().right;
    // A plain-text (non-link) item - eg a grouping label like "Dialog boxes" -
    // has no inline element of its own to measure; the wrapping div is a block
    // stretched to fill #contents, so measure the text node itself via a Range.
    for( let child of item.get(0).childNodes )
    {
        if( child.nodeType===3 && child.textContent.trim() )
        {
            let range=document.createRange();
            range.selectNodeContents(child);
            return range.getBoundingClientRect().right;
        }
    }
    return item.get(0).getBoundingClientRect().left;
}

function fitSidebarToContent()
{
    // Measure against the whole tree, not just whatever happens to be expanded
    // right now - otherwise expanding any collapsed branch afterward would
    // immediately be too wide again, needing a second double-click. Collapsed
    // levels have no layout box at all (display:none), so force them all open
    // just long enough to measure, then restore the original state - this all
    // happens synchronously, before the browser's next paint, so it's not
    // visible to the user.
    let menu=$("#menu");
    let closedLevels=$(".contents-level.closed");
    closedLevels.removeClass("closed");
    let menuLeft=menu.offset().left;
    let maxRight=menuLeft;
    $(".contents-item").each(function(){
        let right=contentsItemRightEdge($(this));
        if( right>maxRight ) maxRight=right;
    });
    closedLevels.addClass("closed");
    if( maxRight>menuLeft ) menu.width(maxRight-menuLeft+15);
}

function installResizeHandle()
{
    let menu=$("#menu");
    $("#resize-handle").on("dblclick",function(event){
        event.preventDefault();
        fitSidebarToContent();
    });
    $("#resize-handle").on("mousedown",function(event){
        event.preventDefault();
        // Defensively clear any handlers/overlay left over from an earlier
        // cycle that didn't clean up - otherwise a stale overlay can sit on
        // top of the divider indefinitely, silently blocking every later
        // click/drag until the page is reloaded.
        endResize();
        // The overlay (see below) must only appear once an actual drag starts,
        // not on mousedown itself - a plain click has no movement in between,
        // and if the overlay already existed at mouseup it would be the click's
        // resolved target instead of #resize-handle, breaking click/dblclick.
        let overlay=null;
        $(document).on("mousemove.resize",function(event){
            if( ! overlay )
            {
                $("body").addClass("resizing");
                // The iframe is a separate document, so it never sees mousemove/
                // mouseup dispatched to this one - a same-document overlay on
                // top of it (rather than eg disabling the iframe's pointer-
                // events) keeps every drag event reaching our own handlers, and
                // leaves the iframe itself untouched so it doesn't need any
                // hover/hit-test state restored once the drag ends. It also
                // gets its own mouseup handler as a redundant safety net: if
                // this document-level one somehow doesn't fire, the overlay -
                // being whatever the mouseup actually lands on - still can.
                overlay=$("<div>").attr("id","resize-overlay").appendTo("body");
                overlay.on("mouseup",endResize);
            }
            menu.width(event.pageX-menu.offset().left);
        });
        $(document).on("mouseup.resize",endResize);
    });
}

function endResize()
{
    $(document).off(".resize");
    $("#resize-overlay").off("mouseup").remove();
    $("body").removeClass("resizing");
}

function installContents()
{
    $(".contents-level").each(function(){
        let level=$(this); 
        if( level.find('.contents-level').length){ level.addClass("has-contents")};
    });
    $(".contents-level").addClass("closed");
    $(".contents-item").prepend($("<div>").addClass("contents-level-toggle"));
    // $(".contents-level-toggle").click(function(){
    //     toggleContentsLevel($(this).closest(".contents-level"));
    // });
    $(".contents-item").click(function(event){
        toggleContentsLevel($(this).closest(".contents-level"))
    });
    $(".contents-item a").each(function(event){
        let link=$(this);
        let level=link.closest(".contents-level");
        let target=link.attr("href");
        $(this).click(function(){ //event){
            // event.preventDefault();
            // event.stopPropagation();
            setPage(target,link);
            openContentsLevel(level);
            return false;
        });
    });

}

function moveContentsSelector( offset )
{
    let items=$('.contents-item').filter(function(nitem,item){
        return $(item).closest('.contents-level').parents('.contents-level.closed').length == 0}
        );
    let selected = items.filter('.selected').first();
    if( selected.length > 0 )
    {
        let nextindex=items.index(selected)+offset;
        if( nextindex >= 0 )
        {
            let next = items.get(items.index(selected)+offset);
            if( next !== undefined )
            {
                next=$(next);
                selected.removeClass('selected');
                next.addClass('selected');
            }
        }
    }
}

function contextKeyEvent( event )
{
    let selected=$('.contents-item.selected').first();
    switch( event.key )
    {
        case "Enter": 
            let link = selected.find('a').first();
            if( link.length )
            {
                setPage(link.attr('href'),link);
            }
            break;
        case "Left":
        case "ArrowLeft":
            {
                let level=selected.closest(".contents-level").parent();
                if( level.hasClass('contents-level') )
                {
                    closeContentsLevel(level);
                    selected.removeClass("selected");
                    level.find(".contents-item").first().addClass("selected");
                }
            }
            break
        case "Right":
        case "ArrowRight":
            {
                let level=selected.closest(".contents-level");
                openContentsLevel(level);
                let next = level.find(".contents-level").first();
                if( next.length > 0 )
                {
                    selected.removeClass('selected');
                    next.find('.contents-item').first().addClass('selected');
                }
            }
            break;
        case "Up":
        case "ArrowUp":
            moveContentsSelector(-1);
            break;
        case "Down":
        case "ArrowDown":
            moveContentsSelector(1);
            break;
        default:
            return;
    }
    event.preventDefault();
    event.stopPropagation();
    return false;
}

function searchText()
{
    return $('#search-text').val().toLowerCase().trim();
}

function searchPages(searchtext)
{
    let words=searchtext.split(/\s+/);
    let pageids=undefined;
    let missing=[];
    for( let word of words )
    {
        // Word not found in index
        let indexid=wordindex.words.get(word);
        if( indexid === undefined ) missing.push(word);
        if( missing.length > 0 ) continue;
        let entry=wordindex.index[indexid];
        // Word is a common word not indexed
        if( entry === null ) continue;
        let npage=entry[0];
        let wordcount=entry[1];
        // Crude weighting - up the weight if in few pages or less common word
        let weightfactor=1.0/Math.sqrt(npage*wordcount);
        let pages=entry[2];
        let newpageids=new Map();
        for( let page of pages )
        {
            let pageid=page[0];
            let pagecount=page[1];
            let weight = pageids === undefined ? 0.0 : pageids.get(pageid);
            if( weight !== undefined )
            {
                weight += pagecount*weightfactor;
                newpageids.set(pageid,weight);
            }
        }
        pageids=newpageids
    }
    let result={"status":"",pages: []};
    if( missing.length > 0 )
    {
        result.status="The following words are not in the index: "+missing.join(", ");
    }
    else if ( pageids === undefined || pageids.size == 0 )
    {
        result.status="No pages matched the search";
    }
    else
    {
        ids=Array.from(pageids.keys());
        ids.sort((id1,id2) => pageids.get(id2)-pageids.get(id1));
        result.pages=ids.map(id => wordindex.pages[id]);
    }
    return result;
}

function searchPageResult( page )
{
    let result=$('<div>').addClass('search-item');
    result.append($('<a>').attr("href",page.url).text(page.title));
    result.click(function(event){ 
        event.preventDefault();
        event.stopPropagation();
        setPage(page.url);
        return false; });
    return result;
}

function doSearch()
{
    let searchtext=searchText();
    if( searchtext == "" ) return;
    let searchResult=searchPages(searchtext);
    let items=searchResult.pages.map( page => searchPageResult(page));
    if( searchText() == searchtext )
    {
        $('#search-words').empty();
        let results=$('#search-results');
        results.empty();
        if( items.length > 0)
        {
            results.append(items);
            $('#search-results .search-item').first().addClass('selected');
        }
        else
        {
            results.text(searchResult.status)
        }
    }
}

function lookupWords()
{
    let search=$("#search-text");
    let text=search.val();
    let index=search.prop("selectionStart");
    let wordprefix=text.substring(0,index).replace(/^.*\s/,'').toLowerCase();
    let wordsuffix=text.substring(index).replace(/[^\s]*/,'');
    let prefixlen=wordprefix.length;
    if( wordprefix.length > 1 )
    {
        $('#search-results').empty();
        let wordlist=$('#search-words');
        let words=wordindex.wordlist.filter(w=>w.startsWith(wordprefix)).sort();
        wordlist.empty();
        for( let word of words)
        {
            let suggestion=$("<div>").addClass("search-word").text(word);
            suggestion.click(function(){
                if(search.val()==text)
                {
                    search.val(text.substring(0,index-prefixlen)+word+wordsuffix);
                    search.focus();
                    search.prop('selectionStart',index+word.length-prefixlen);
                    search.prop('selectionEnd',index+word.length-prefixlen);
                    wordlist.empty();
                    // Match what pressing Enter on a selected word does - fill
                    // it in and immediately search, rather than leaving the
                    // mouse and keyboard paths with different behaviour.
                    doSearch();
                }
            })
            wordlist.append(suggestion);
        }
        wordlist.find('.search-word').first().addClass('selected') 
    }
}

function moveSelected( selector, offset )
{
    let items=$(selector);
    let selected=items.filter('.selected').first();
    if( selected.length > 0 )
    {
        let index=items.index(selected)+offset;
        let next=items.get(index);
        if( index >= 0 && next !== undefined )
        {
            selected.removeClass('selected');
            $(next).addClass('selected')
        }
    }
}


function installSearch()
{
    wordindex.wordlist=Array.from(wordindex.words.keys());
    let searchpanel=$('#search');
    let searchbar=$("<div>").addClass("search-bar").attr("id","search-bar");
    let searchtext=$("<input>").addClass("search-text").attr("id","search-text");
    let searchbutton=$("<div>").addClass("search-button").attr("id","search-button");
    searchtext.on("keydown", function(e) {
        switch( e.key) 
        {
            case "Enter":
                // If already showing a result then click it
                let result=$('#search-results .search-item.selected').first();
                if( result.length > 0)
                {
                    result.click();
                }
                else
                {
                    $('#search-words .search-word.selected').first().click();
                    doSearch();
                }
                break;
            case "Up":
            case "ArrowUp":
                moveSelected('#search-results .search-item',-1);
                moveSelected('#search-words .search-word',-1);
                break;
            case "Down":
            case "ArrowDown":
                moveSelected('#search-results .search-item',1);
                moveSelected('#search-words .search-word',1);
                break;
            default:
                return
        }
        e.preventDefault();
        e.stopPropagation();
        return false
    });
    searchtext.on("keyup", function(e){
        switch(e.key)
        {
            case "Enter":
            case "Up":
            case "ArrowUp":
            case "Down":
            case "ArrowDown":
                e.preventDefault();
                e.stopPropagation();
                return false                
        }
        lookupWords();
    });
    searchbutton.click(doSearch);
    searchbar.append(searchtext,searchbutton);
    searchpanel.append(searchbar);
    searchpanel.append($("<div>").addClass("search-words").attr("id","search-words"));
    searchpanel.append($("<div>").addClass("search-results").attr("id","search-results"));
}

function setup()
{
    if( ready ) return;
    ready=true;
    $('#show-contents-button').click(function(){ 
        $('#search').hide(); 
        $('#show-search-button').removeClass('selected');
        $('#contents').show(); 
        $('#show-contents-button').addClass('selected'); 
    });
    $('#show-search-button').click(function(){ 
        $('#contents').hide(); 
        $('#show-contents-button').removeClass('selected'); 
        $('#search').show();
        $('#show-search-button').addClass('selected');
        $('#search-text').focus();
        $('#search-text').select();
    });
    $('#show-contents-button').click();
    installContents();
    installResizeHandle();
    let url= window.location.search ? window.location.search.substring(1) : $('.contents-item a').first().attr("href");
    setPage(url);
    $(window).on("message",function(e){
        // The browser already creates its own joint-history entry for every
        // iframe navigation (sidebar/search click, in-page content link, or a
        // back/forward traversal restoring one) - whether we drove it or the
        // user did. So this only ever labels the entry the browser just made;
        // it must never pushState, or it would add a redundant second entry on
        // top of that implicit one (which is what used to make Back land on a
        // stale entry whose address had changed but whose content hadn't).
        let msg=e.originalEvent;
        if( ! msg.data || msg.data.type !== "snap-help-page" ) return;
        if( msg.source !== document.getElementById('help-page').contentWindow ) return;
        let pageurl=helpRootRelativeUrl(msg.data.url);
        highlightPage(pageurl);
        history.replaceState({page:pageurl},"","?"+pageurl);
    });
    $(window).on("keydown",function(e){
        if( e.altKey && e.key == 'c' )
        {
            $('#show-contents-button').click();
        }
        else if( e.altKey && e.key == 's' )
        {
            $('#show-search-button').click();
        }
        else if( $('#show-contents-button').hasClass('selected'))
        {
            contextKeyEvent(e);
        }
    });
}

$(document).ready(setup);