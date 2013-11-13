  
#######################################################################
# Utility functions

#######################################################################
# Add and remove elements from list

function AddToList( $list, $newelements )
	$list = "\n".$list
	foreach $new in $newelements do
		$expression = '(?n)^'.Replace($new,'(\W)','\\\1').'$'
		if not Match($list,$expression) then
			$list = $list."\n".$new
		endif
	endforeach
	return  Replace($list,'\A\n','')
end_function

function RemoveFromList( $list, $badelements )
	$newlist = ''
	foreach $element in $list do
		$expression = '(?n)^'.Replace($element,'(\W)','\\\1').'$'
		if not Match($badelements,$expression) then
			$newlist = $newlist."\n".$element
		endif
	endforeach
	return Replace($newlist,'\A\n','')
end_function


#######################################################################
# Copy a template file, replacing $xxxx strings with corresponding
# system or global variables...
# or parameter defined as !name!value!name!value... where ! is any 
# character

function CopyTemplate($template,$newfile,$params)
    $templatetext = ReadFile( "$script_path/$template.template" )
    $newtext = ''
    $delim = Match($params,'.')
    if not Match($delim,'\w') then $delim = '\'.$delim endif
    $skipre = ''
    if $delim and $params then
        $skipre = $delim.'[^'.$delim.']*'
        $skipre = "(?:$skipre$$skipre$)*"
    endif
    foreach $part in $templatetext delimited_by '(\$\w+)' do
       if Match($part,'\$\w+') then 
            $value = GetValue($part)
            if not $value and $skipre then
                $param = Match($part,'\$(.*)',1)
                $re = '(?i)^'.$skipre.$delim.$param.$delim.'([^'.$delim.']*)'
                $value = Match($params,$re,1)
            endif
            $part = $value
       endif
       $newtext = $newtext.$part
    endforeach
    return WriteFile($newfile,$newtext)
end_function

######################################################################
# FindPerl - finds and returns the path to the perl program
# or puts up an error message and returns an empty string if not found

function FindPerl()
    $perl = FindProgram("perl.exe")
    if $perl then return $perl endif
    Message("Perl must be installed on the computer to use this function","Perl missing")
    return ''
end_function

#######################################################################
# Debug functions ... generate output if $_debug evaluates to true,
# otherwise do nothing.

function debuglog( $message )
  if $_debug then log( $message ) endif
end_function 

function debuglogrun( $command )
  if $_debug then log("\nRunning $command") endif
  $output = LogRun( $command )
  if $_debug then log("\nOutput:\n") log($output) endif
  return $output
end_function

function debugfile( $name, $file )
  if $_debug then
     log("\n====================================\nFile $name:")
     log(ReadFile($file))
     log("\n====================================\n")
  endif
end_function  

