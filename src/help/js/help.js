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
    });
    $('#show_contents_button').click();
    installContents();
}

$(document).ready(setup);