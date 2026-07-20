window.addEventListener( "pageshow", function()
{
    if( window.parent !== window )
    {
        window.parent.postMessage( { type: "snap-help-page", url: location.href }, "*" );
    }
});
