

$_data_file_re = '(?ni)^\s*data_file\s+(\S+)(?:\s+error_factor\s+(\d+(?:\.\d*)?))?(?:\s+(csv)(?:\s+(.*\S))?)?\s*$'
$_data_file_prefix = 'data_file '

function DataFileList()
   if not $job_file then return '' endif
   $files = ReadFile( $job_file, $_data_file_re )
   # Normalize file prefix to simplify matching ...
   $files = Replace($files,'(?ni)^\s*data_file\s+',$_data_file_prefix)
   return $files
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
   
function EditDataFileOptions( $dfd )
   $csvtypere = '(?i)\yformat\=(\S+)'
   $filename = Match($dfd,$_data_file_re,1)
   $factor = Match($dfd,$_data_file_re,2)
   $csv = Match($dfd,$_data_file_re,3)
   $csvopts = Match($dfd,$_data_file_re,4)
   $csvtype = Match($csvopts,$csvtypere,1)
   $csvopts = Replace($csvopts,$csvtypere,'')
   Log("File: $filename")
   Log("Factor: $factor")
   Log("CSV: $csv")
   Log("Type: $csvtype")
   Log("Options: $csvopts")
   return $dfd
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
  $addfiles = GetOpenFilename("SNAP data files","Data files (*.dat)|*.dat|CSV files (*.csv)|*.csv|All files (*.*)|*.*",true)
  if not $addfiles then return '' endif
  $newfiles = ''
  foreach $addfile in $addfiles do
     $newfiles = $newfiles . $_data_file_prefix.Filename($addfile,"relative",$jobpath) ."\n"
  endforeach
  $options = GetDataFileOptions( $newfiles )
  if not $options then return '' endif
  $options = Replace($options,'^filename','')
  if $options then
      $options = Replace($options,'(\\|\&)','\\\0')
      $newfiles = Replace($newfiles,'\n',$options."\n")
  endif 
  return $newfiles
end_function

######################################################################
# Functions to edit or plot a list of datafiles.  Edit function opens
# a new editor for each file.  Plot puts them all into a single 
# snapplot job

function EditDataFiles( $datafilelist )
        foreach $df in $datafilelist do
            start($_editor,$df)
        endforeach
end_function

function PlotDataFiles( $datafilelist )
      if $datafilelist then
         $dflist = Replace($datafilelist,"\n"," ")
         start("$snap_path/snapplot","-f",$coordinate_file,$dflist)
      endif
end_function

######################################################################
# Functions to edit options for data files

function GetDataFileOptions( $jobpath, $filelist )
   $csvtypere = '(?i)\yformat\=(\S+)\s*'
   $filename = Match($filelist,$_data_file_re,1)
   $factor = Match($filelist,$_data_file_re,2)
   $csv = Match($filelist,$_data_file_re,3)
   $csvopts = Match($filelist,$_data_file_re,4)
   $csvtype = Match($csvopts,$csvtypere,1)
   $csvopts = Replace($csvopts,$csvtypere,'')
   if $csvtype then $csvtype = 'csv format='.$csvtype endif
   dialog "Edit data file settings" $ok
      label "Files: ".DataFileNamesFromList($filelist)
       dropdown_selector $csvtype  "Format: " DataFileFormatList($csvtype,$jobpath)
       textbox $csvopts "CSV configuration (name=value ...)" "20"
       textbox $factor "Error factor:" '10::~0123456789.~(?:\d+\.?\d*)?~Error factor must be a number'
   end_dialog
 
   $options = ''
   if $ok then 
       $options = 'filename'
       if $factor then $options = ' error_factor '.$factor endif
       if $csvtype then $options = $options.' '.$csvtype.' '.$csvopts endif
   endif
   return $options
end_function

function CSVDataTypesOnPath( $path )
   $types = ''
   foreach $type in Directory($path, '*.dtf' ) do
      if not ReadFile( $path.'/'.$type,'(?i)^\s*observation\s*$',1) then
          continue
      endif
      $typecode = Replace($type,'(?i)\.dtf','')
      $typename = ReadFile( $path.'/'.$type,'(?i)^\s*format_name\s+',1 )
      if $typename then
         $typename = Replace(Replace($typename,'(?i)\s*format_name\s+(.*?)\s*','\1'),'\~','')
      else
         $typename = $typecode." CSV format"
      endif
      $types = $types.'~csv format='.$typecode.'~'.$typename
   endforeach
   return $types
end_function

function DataFileFormatList( $filetype, $jobpath )
   $typelist = "~~SNAP data format".CSVDataTypesOnPath($snap_path).CSVDataTypesOnPath($jobpath)
   $filetypere = '\~'.Replace($filetype,'\W','\\\0').'\~'
   if not Match($typelist,$filetypere) then
      $typelist = $typelist."~$filetype~$filetype format"
   endif
   return $typelist
end_function

function ListBoxFileList( $datafiles )
   return Replace("\n".Replace($datafiles,'\n$',''),'\n'.$_data_file_prefix,"\n")
end_function

function DataFileListFromSelected( $listboxfiles )
   return Replace(Replace("\n".$listboxfiles,'\n',"\n".$_data_file_prefix),'^\n','')."\n"
end_function

function DataFileNamesFromList( $filelist )
   return Replace(Replace( $filelist, $_data_file_re, '\1' ),'\s+',' ')
end_function


function UpdateDataFilesInJob( $datafiles )
   $jobfilecontent = ReadJobFile()
   $comment_re = '(?ni)^\s*\!'
   $dfcomment_re = '(?ni)^\s*\!\s+data\s+files?\s*$'
   $nonblankre = '\S'
   $newcontent = ''
   $pending=false
   $nonblank=false
   foreach $line in $jobfilecontent do
      if Match($line,$comment_re) then
          if Match($line,$dfcomment_re) then 
              $pending=true
	      $datafiles="\n".$datafiles
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
        $datafiles = $datafiles .CreateNewDataFile($jobpath)
        $listfiles = ListBoxFileList($datafiles)
    button "Add ..."
        $datafiles = $datafiles . AdditionalDataFiles($jobpath)
        $listfiles = ListBoxFileList($datafiles)
    button "Remove ..."
        if $selectedfiles then
        if ask("Remove these data files from the job\n".DataFileNamesFromList(DataFileListFromSelected($selectedfiles)),"Confirm") then
            foreach $file in $selectedfiles do
                $datafiles = Replace($datafiles,"(?q)".$_data_file_prefix.$file."\n","")
            endforeach
        endif
        $listfiles = ListBoxFileList($datafiles)
        endif
    button "EditOptions"
        if $selectedfiles then 
            $options = GetDataFileOptions( $jobpath, DataFileListFromSelected($selectedfiles) )
            if $options then 
                $options = Replace($options,'^filename','')
                $options = Replace($options,'(\\|\&)','\\\0')
                foreach $file in $selectedfiles do
                   $newfile = Replace($_data_file_prefix.$file,$_data_file_re,$_data_file_prefix.'\1 '.$options)."\n"
                   $datafiles = Replace($datafiles,"(?nq)".$_data_file_prefix.$file."\n",$newfile)
                endforeach
            endif
        endif
        $listfiles = ListBoxFileList($datafiles)
    button "Edit"
        EditDataFiles( DataFileNamesFromList(DataFileListFromSelected($selectedfiles)) )
    button "Plot"
        PlotDataFiles( DataFileNamesFromList(DataFileListFromSelected($selectedfiles)) )
    spacer
    end_dialog
    if $ok and $job_file then
        UpdateDataFilesInJob( $datafiles )
    endif
    return $datafiles
end_function
