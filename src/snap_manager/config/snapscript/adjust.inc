#######################################################################
# Functions relating to running and analysing adjustments
#######################################################################
# Function to run an adjustment

function RunAdjustment()
   # Delete output files so the we know any files afterward are current
   DeleteFile(SetExtension($coordinate_file,"new"))
   DeleteFile(FindJobFile("lst"))
   DeleteFile(FindJobFile("err"))
   
   debugrun("$snap_path/snapadjust",$job_file)
   $error_file = FindJobFile("err")
   if $error_file then
      if Ask("SNAP has reported errors with this job\r\nView the error file","SNAP errors") then
         start($_editor,$error_file)
      endif
   endif
end_function   
 
#######################################################################
# Function to plot an adjustment
 
function PlotAdjustment()
   start("$snap_path/snapplot",$job_file)
end_function
 
#######################################################################
# Function to view a listing file

function ViewAdjustmentListing()
   start($_editor, FindJobFile("lst"))
end_function
 
#######################################################################
# Function to view an error file

function ViewAdjustmentErrorLog()
   start($_editor, FindJobFile("err"))
end_function
 
#######################################################################
# Function to update coordinates from adjustment

function UpdateAdjustmentCoordinates()
    if not CheckCoordinateFile(true) then exit endif
    BackupCoordinateFile()
    RenameFile(SetExtension($coordinate_file,"new"),$coordinate_file)
    $relname = Filename($coordinate_file,"relative",$job_path)
    log("\nCoordinate file $relname updated with adjustment results")
end_function
 
#######################################################################
# Function to calculate coordinate orders

function CalculateCoordinateOrders()
   $usectrl = true
   $newcrd = true
   $maxorder = "5"
   
   dialog "Calculate orders"
   label "Use snapspec to calculate orders of adjusted stations"
   label "based on horizontal relative accuracy"
   spacer
   textbox  $maxorder "Lowest order to calculate" "5::~0123456789~^[0-9]\$~Order must be a single digit"
   spacer
   checkbox $usectrl "Only calculate orders lower than control station orders"
   checkbox $newcrd "Update coordinate file with calculated orders"
   spacer
   end_dialog
   
   $binfile = FindJobFile("bin")
   $lstfile = "snapspec.lst"
  
   if $maxorder then $maxorder = "-o $maxorder" endif
   if $usectrl then $usectrl = "-a" endif
   $newcrdfile = ""
   if $newcrd then
      if not BackupCoordinateFile($coordinate_file) then
         message("Could not back up coordinate file","Error")
         exit
      endif
      $newcrdfile = TempFile()
      DeleteFile($newcrdfile)
      $newcrd = "-u \"$newcrdfile\""
   endif
   $output = debuglogrun("\"$snap_path/snapspec\" $maxorder $usectrl $newcrd \"$binfile\" \"$lstfile\"")
   
   if not FileExists($lstfile) then
      message("Snapspec failed\n\n$output","Error")
      exit
   endif
   
   $message = "Snapspec output in snapspec.lst"
   if $newcrdfile then
      if FileExists($newcrdfile) then
         RenameFile($newcrdfile,$coordinate_file)
         log("\nCoordinate file updated with new orders by snapspec")
      else
         $message = $message . "\n\nSnapspec could not calculate new orders"
      endif
   endif
   
   if ask($message."\n\nDo you want to view the listing file\n","View report") then
      start($_editor,$lstfile)
   endif
end_function
 
