var wordindex=null;
var ready=false;

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

function setPage( url )
{
    $('#help-page').attr("src",url); 
    $(".contents-item").removeClass("selected");
    $(".contents-item a").each(function(){
        if( $(this).attr("href") == url )
        {
            $(this).closest(".contents-item").addClass("selected");
            $(this).parents(".contents-level").each(function(){openContentsLevel($(this));});
        }
    });
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
            setPage(target);
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
                setPage(link.attr('href'));
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
    let url= window.location.search ? window.location.search.substring(1) : $('.contents-item a').first().attr("href");
    setPage(url);
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