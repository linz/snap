
function installHelp()
{
    $(".contents-item a").each(function(){
        let link=$(this);
        let target=link.attr("href")
        $(this).click(function(event){
            event.preventDefault();
            $("#help-page").attr("src",target);
        });
    });

}

$(document).ready(installHelp);