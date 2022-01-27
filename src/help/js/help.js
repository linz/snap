var wordindex=null;

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

// function onPageLoad()
// {
//     // Doesn't work - haven't found a way to get url of page.
    
//     let helppage=$('#help-page');
//     let target=helppage.attr("src");
//     $(".contents-item").removeClass("selected");
//     $(".contents-item a").each(function(){
//         if( $(this).attr("href") == target )
//         {
//             $(this).closest(".contents-item").addClass("selected");
//             $(this).parents(".contents-level").each(function(){openContentsLevel($(this));});
//         }
//     });
// }



function installContents()
{
    let helppage=$("#help-page");
    // helppage.on("load",onPageLoad);
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
        $(this).click(function(event){
            event.preventDefault();
            event.stopPropagation();
            if( helppage.attr("src") == target )
            {
                toggleContentsLevel(level);
            }
            else
            {
                helppage.attr("src",target);
                openContentsLevel(level);
            }
        });
    });

}

function searchText()
{
    return $('#search_text').val().trim();
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
    let result=$('<div>').addClass('search_item');
    result.append($('<a>').attr("href",page.url).text(page.title));
    result.click(function(){ $('#help-page').attr("src",page.url); return false; });
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
        let results=$('#search_results');
        results.empty();
        if( items.length > 0)
        {
            results.append(items);
        }
        else
        {
            results.text(searchResult.status)
        }
    }
}

function installSearch()
{
    let searchpanel=$('#search');
    let searchbar=$("<div>").addClass("search_bar");
    let searchtext=$("<input>").addClass("search_text").attr("id","search_text");
    let searchbutton=$("<div>").addClass("search_button").attr("id","search_button");
    searchtext.on("keypress", function(e) {
        if (e.keyCode == 13) { 
            doSearch();
            return false; 
        }
    });
    searchbutton.click(doSearch);
    searchbar.append(searchtext,searchbutton);
    searchpanel.append(searchbar);
    searchpanel.append($("<div>").addClass("search_preview").attr("id","search_preview"));
    searchpanel.append($("<div>").addClass("search_results").attr("id","search_results"));
}

function setup()
{
    $('#show_contents_button').click(function(){ 
        $('#search').hide(); 
        $('#show_search_button').removeClass('selected');
        $('#contents').show(); 
        $('#show_contents_button').addClass('selected'); 
    });
    $('#show_search_button').click(function(){ 
        $('#contents').hide(); 
        $('#show_contents_button').removeClass('selected'); 
        $('#search').show();
        $('#show_search_button').addClass('selected');
        $('#search_text').focus();
        $('#search_text').select();
    });
    $('#show_contents_button').click();
    installContents();
}

$(document).ready(setup);