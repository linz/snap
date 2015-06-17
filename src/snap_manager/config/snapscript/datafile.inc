

$_data_file_re = 
    '(?ni)^\s*data_file\s+(\S+)(?:'.
    '(?:\s+error_factor\s+(\d+(?:\.\d*)?))?|'.
    '(?:\s+recode\s+(\S+))?|'.
    '(?:\s+(csv|sinex)(?:\s+\w+\=\S+)*)?'.
    ')*\s*$' 

$_data_file_df_re='(?ni)^\s*data_file\s+(\S+)'
$_data_file_ef_re='(?ni)\s+error_factor\s+(\d+(?:\.\d*)?)'
$_data_file_rc_re='(?ni)\s+recode\s+(\S+)'
$_data_file_csv_re='(?ni)\s+(csv)((?:\s+\w+\=\S+)*)'
$_data_file_snx_re='(?ni)\s+(sinex)((?:\s+\w+\=\S+)*)'

$_data_file_prefix = 'data_file '

# Extract the files from the command file.  Note: does not handle included files :-(

function DataFileList()
   if not $job_file then return '' endif
   $files = ReadFile( $job_file, $_data_file_re )
   # Normalize file prefix to simplify matching ...
   $files = Replace($files,'(?ni)^\s*data_file\s+',$_data_file_prefix)
   return $files
end_function

# Update a file list by replacing definitions from $updates, or adding them
# at the end if not required

function UpdateDataFileList( $filelist, $updates )
    $filesmatched=""
    $result=""
    foreach $f in $filelist do
        $filename=Match($f,$_data_file_re,1)
        if not $filename then continue endif
        $used=false
        foreach $fm in $filesmatched do if $fm == $filename then $used=true endif endforeach
        if $used then continue endif
        foreach $fu in $updates do 
            $ffu=Match($fu,$_data_file_re,1)
            if $ffu == $filename then $f = $fu break endif
        endforeach
        $filesmatched=$filesmatched.$filename."\n"
        $result=$result.$f."\n"
    endforeach
    foreach $fu in $updates do 
        $filename=Match($fu,$_data_file_re,1)
        if not $filename then continue endif
        $used=false
        foreach $fm in $filesmatched do if $fm == $filename then $used=true endif endforeach
        if $used then continue endif
        $filesmatched=$filesmatched.$filename."\n"
        $result=$result.$fu."\n"
    endforeach
    $result=Replace($result,'\n$','')
    debuglog("RESULT:$result:")
    return $result
end_function

function RemoveFromDataFileList( $filelist, $remove )
    $result=""
    foreach $f in $filelist do
        $filename=Match($f,$_data_file_re,1)
        if not $filename then continue endif
        $use=true
        foreach $fu in $remove do 
            $ffu=Match($fu,$_data_file_re,1)
            if $ffu == $filename then $use = false break endif
        endforeach
        if $use then
            $result=$result.$f."\n"
        endif
    endforeach
    $result=Replace($result,'\n$','')
    return $result
end_function

function AddDataFile( $datfile, $options )
   $datafiles=DataFileList()
   $newfiles=""
   foreach $dfd in $datafiles do
       $filename=Match($dfd,$_data_file_re,1)
       if $filename=='' then continue endif
       if $filename == $datfile then continue endif
       $newfiles=$newfiles.$dfd."\n"
   endforeach
   if $options != '' then
        $datfile=$datfile.' '.$options
   endif
   $newfiles=$newfiles.$_data_file_prefix.$datfile."\n"
   UpdateDataFilesInJob($newfiles)
end_function

function ProcessDataFile( $datafiles,$dfspec,$action )
   $result = ""
   foreach  $dfd in $datafiles do
      if $dfd == $dfspec then 
         $filename = ""
         if $action == "editoptions" then 
             $dfd = EditDataFileOptions($dfd)
         elseif $action == "delete" then
             continue
         endif
      endif
      $result = $result.$dfd."\n"
   endforeach
   return $result
end_function
   
function BuildDataFileDef( $filename,$filetype,$fileopts,$factor,$recode )
   $dfd=$_data_file_prefix.$filename
   if $filetype then 
      $dfd="$dfd $filetype"
      if $fileopts then
         $dfd="$dfd $fileopts"
      endif
   endif
   if $factor then
       $dfd="$dfd error_factor $factor"
   endif
   if $recode then
        $dfd="$dfd recode $recode"
   endif
   return $dfd
end_function

function ParseDataFileDef( $dfd )
   $csvtypere = '(?i)\sformat\=(\S+)'
   $trimre='^\s*(.*?)\s*$'
   $filename = Match($dfd,$_data_file_df_re,1)
   $factor = Match($dfd,$_data_file_ef_re,1)
   $recode = Match($dfd,$_data_file_rc_re,1)
   $filetype, $fileopts = MatchGroups($dfd,$_data_file_csv_re)
   if $filetype  then
       $filetype='csv'
       $csvtype = Match($fileopts,$csvtypere,1)
       $fileopts = Replace($fileopts,$csvtypere,'')
       if $csvtype then
          $filetype="csv format=$csvtype"
       endif
   else
       $filetype, $fileopts = MatchGroups($dfd,$_data_file_snx_re)
       $filetype=LowerCase($filetype)
   endif
   $fileopts=Replace($fileopts,'\s+',' ')
   $fileopts=Replace($fileopts,'^\s+','')
   $dfd=BuildDataFileDef($filename,$filetype,$fileopts,$factor,$recode)
   return $dfd, $filename,$filetype,$fileopts,$factor,$recode
end_function

function CreateNewDataFile($jobpath)
   $datafile = ""
   $datafile = GetSaveFilename(
           "Create new SNAP data file",
           "SNAP data files (*.dat)|*.dat"
   )

   if not $datafile then return '' endif

   $_title = $job_title
   CopyTemplate("default.dat",$datafile)
   $datafile = $_data_file_prefix.Filename($datafile, "relative", $jobpath)."\n"
   return $datafile
end_function

function AdditionalDataFiles($jobpath)
  $addfiles = GetOpenFilename("SNAP data files","Data files (*.dat)|*.dat|CSV files (*.csv)|*.csv|SINEX files (*.snx)|*.snx|All files (*.*)|*.*",true)
  if not $addfiles then return '' endif
  $newfiles = ''
  foreach $addfile in $addfiles do
     $filetype=""
     if Match($addfile,'(?i)\.csv') then $filetype=" csv" endif
     if Match($addfile,'(?i)\.snx') then $filetype=" sinex" endif
     $newfiles = $newfiles . $_data_file_prefix.Filename($addfile,"relative",$jobpath) .$filetype."\n"
  endforeach
  return $newfiles
end_function

######################################################################
# Functions to edit or plot a list of datafiles.  Edit function opens
# a new editor for each file.  Plot puts them all into a single 
# snapplot job

function EditDataFiles( $datafilelist )
        foreach $df in $datafilelist delimited_by '\s+' do
            start($_editor,$df)
        endforeach
end_function

function PlotDataFiles( $datafiles )
      if not Match($datafiles,'\S') then
          Message("You must select some files to plot!","Plot error")
          return false
      endif
      $files=''
      $badfiles=''
      foreach $f in $datafiles do
          $dfd,$filename,$filetype,$fileopts,$factor,$recode=ParseDataFileDef($f)
          if $filetype then
            $badfiles=$badfiles.' '.$filename
          else
            $files=$files.' '.$filename
          endif
      endforeach
      if $badfiles then
          Message("Only SNAP format files can be plotted from the data file dialog.\n".
              "The files".$badfiles." cannot be plotted.\n".
              "Use the Adjust | Plot menu option to plot other formats.","Plot error")
      endif
      if $files then
         start("$snap_path/snapplot","-f",$coordinate_file,MatchGroups($files,'(\S+)',true))
      endif
end_function

######################################################################
# Functions to edit options for data files

function EditDataFileOptions( $jobpath, $filelist )
   $filetype=""
   $fileopts=""
   $factor=""
   $recode=""
   $result=$filelist
   foreach $f in $filelist do
       $dfd,$filename,$filetype,$fileopts,$factor,$recode=ParseDataFileDef($f)
       break
   endforeach
   $formatlist=DataFileFormatList($filetype,$jobpath)
   
   dialog "Edit data file settings" $ok
       label "Files: ".DataFileNamesFromList($filelist)
       dropdown_selector $filetype  "Format: " $formatlist
       textbox $fileopts "CSV/SINEX configuration (name=value ...)" 
           '40::~~^(\s*\w+\=\S+(\s+\w+\=\S+)*)?\s*$~CSV/SINEX configuration must be name=value name=value ...'
       textbox $factor "Error factor:" '10::~0123456789.~(?:\d+\.?\d*)?~Error factor must be a number'
       textbox $recode "Station recode file" '20::~~\S*~Recode must be file name or blank'
       validator
          if Match($fileopts,'\S') and not $filetype then
              Message("CSV/SINEX configuration options don't apply for SNAP format files.\n".
                      "Select a different file type or remove the config options.\n","Config error")
              return false
          endif
          return true
   end_dialog

   if $ok then
       debuglog("Opts result: $fileopts")
       $result=""
       foreach $f in $filelist do
          $dfd,$filename,$filetypex,$fileoptsx,$factorx,$recodex=ParseDataFileDef($f)
          if $filename then
              $dfd=BuildDataFileDef($filename,$filetype,$fileopts,$factor,$recode)
              $result=$result.$dfd."\n"
          endif
       endforeach
   endif
   return $ok,$result
end_function

function CSVDataTypes( $path )
   $cfgtypes=FindConfigFiles('format','dtf')
   $typecodes=''
   $types=''
   foreach $type in $cfgtypes do
      $typecode,$area,$dtf=MatchGroups($type,'^(\S+)\s*\((\S+)\)\s+(.*)$')
      if not FileExists($dtf) then continue endif
      $utype=UpperCase($typecode)
      if Match($typecodes,'\|'.$typecode) then continue endif
      $typecodes=$typecodes.'|'.$typecode

      if not ReadFile( $dtf,'(?i)^\s*observation\s*$',1) then continue endif

      $typename = ReadFile( $dtf,'(?i)^\s*format_name\s+',1 )
      if $typename then
         $typename = Replace(Replace($typename,'(?i)\s*format_name\s+(.*?)\s*','\1'),'\~','')
         $typename = $typename." (".$typecode.")"
      else
         $typename = $typecode." CSV format"
      endif
      $types = $types.'~csv format='.$typecode.'~'.$typename
   endforeach
   return $types
end_function

function DataFileFormatList( $filetype, $jobpath )
   $typelist = "~~SNAP data format".CSVDataTypes($snap_path)
   $filetypere = '\~'.Replace($filetype,'\W','\\\0').'\~'
   if not Match($typelist,$filetypere) then
      $typelist = $typelist."~$filetype~$filetype format"
   endif
   $typelist = $typelist."~sinex~SINEX format"
   return $typelist
end_function

function ListBoxFileList( $datafiles )
   return Replace("\n".Replace($datafiles,'\n$',''),'\n'.$_data_file_prefix,"\n")
end_function

function DataFileListFromSelected( $listboxfiles )
   debuglog("BEGIN:$listboxfiles:")
   $datafiles=Replace(Replace("\n".$listboxfiles,'\n(\S)',"\n".$_data_file_prefix.'\1'),'^\n','')."\n"
   debuglog("END:$datafiles:")
   return $datafiles
end_function

function DataFileNamesFromList( $filelist )
   $filenames=Replace(Replace( $filelist, $_data_file_re, '\1' ),'\s+',' ')
end_function

function UpdateDataFilesInJob( $datafiles )
   $jobfilecontent = ReadJobFile()
   $comment_re = '(?ni)^\s*\!'
   $dfcomment_re = '(?ni)^\s*\!\s+data\s+files?\s*$'
   $nonblankre = '\S'
   $newcontent = ''
   $pending=false
   $nonblank=false
   $datafiles=Replace($datafiles,'\s+$',"")
   $datafiles=$datafiles."\n"
   $datafiles=Replace($datafiles,'^\s+',"")
   # debuglog("JOBFILE:$jobfilecontent:END")
   foreach $line in $jobfilecontent do
      debuglog($line)
      if Match($line,$comment_re) then
          if Match($line,$dfcomment_re) then 
	      if (not $pending) and $datafiles != '' then 
                  $pending=true
	          $datafiles="\n".$datafiles 
	      endif
          endif
          $newcontent=$newcontent.$line."\n"
          continue
      endif
      if Match($line,$_data_file_re ) then
          $pending=true
          continue
      endif
      if $pending then
         $newcontent=$newcontent.$datafiles
         $datafiles=''
         $pending=false
      endif
      if Match($line,$nonblankre) then
          $newcontent = $newcontent.$line."\n"
          $nonblank=true
      else
        if $nonblank then
             $newcontent = $newcontent.$line."\n"
        endif
        $nonblank=false
      endif
   endforeach
   $newcontent = $newcontent.$datafiles
   # debuglog("NEW:$newcontent:END")
   UpdateJobFile($newcontent)
end_function

######################################################################
# Main dialog

function DataFileDialog($jobpath)
    if not $jobpath then $jobpath = $job_path endif
    $datafiles = DataFileList()
    $listfiles = ListBoxFileList($datafiles)
    dialog "Edit/View data files" $ok
    listbox $selectedfiles "Select data file" $listfiles "50x15 extended"
    new_column
    spacer
    button "New ..."
        $datafiles = UpdateDataFileList($datafiles,CreateNewDataFile($jobpath))
        $listfiles = ListBoxFileList($datafiles)
    button "Add ..."
        $newfiles=AdditionalDataFiles($jobpath)
        $datafiles = UpdateDataFileList($datafiles,$newfiles)
        $listfiles = ListBoxFileList($datafiles)
    button "Remove ..."
        if $selectedfiles then
        if ask("Remove these data files from the job\n".DataFileNamesFromList(DataFileListFromSelected($selectedfiles)),"Confirm") then
            $datafiles=RemoveFromDataFileList($datafiles,DataFileListFromSelected($selectedfiles))
            $listfiles = ListBoxFileList($datafiles)
        endif
        endif
    button "Edit Options"
        if $selectedfiles then 
            $editok,$listupdates = EditDataFileOptions( $jobpath, DataFileListFromSelected($selectedfiles) )
            if $editok then 
                $datafiles = UpdateDataFileList($datafiles,$listupdates)
                $listfiles = ListBoxFileList($datafiles)
            endif
        endif
    button "Edit"
        EditDataFiles( DataFileNamesFromList(DataFileListFromSelected($selectedfiles)) )
    button "Plot"
        PlotDataFiles( DataFileListFromSelected($selectedfiles)) 
    spacer
    end_dialog
    if $ok and $job_file then
        UpdateDataFilesInJob( $datafiles )
    endif
    return $datafiles
end_function

# vim: set expandtab shiftwidth=4 softtabstop=4 smarttab:
