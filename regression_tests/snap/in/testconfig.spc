! SNAPPLOT configuration file

error_type aposteriori standard_error
error_scale horizontal 1.00 times_default
error_scale vertical 1.00 times_default
obs_listing_fields from to data_type status std_residual redundancy
obs_listing_order std_residual
observation_options show_obs_directions merge_similar_obs no_show_hidden_station_obs
observation_spacing 1.00 times_default
highlight_observations none

! Station code and name font
station_font arial 10

! Station offsets
ignore_station_offsets yes



! Key defined as follows:

station_colours Mark_Type
observation_colours by_redundancy 5



key "SC_-" on RED
key "SC_BOLT" on GREEN
key "SC_PEG" on RED
key "SC_PIN" on BROWN

key "Free_stations" on
key "Fixed_stations" on
key "Hor_fixed_stns" on
key "Vrt_fixed_stns" on
key "Rejected_stns" on
key "Symbol" on
key "Name" on
key "Code" on






key "RDC_0" on ORANGE
key "RDC_1" on RED
key "RDC_2" on BLUE
key "RDC_3" on YELLOW
key "RDC_4" on CYAN
key "RDC_5" on GREY

key "Slope_distance" on
key "Horizontal_distance" on
key "Ellipsoidal_distance" on
key "Distance_ratio" on
key "Horizontal_angle" on
key "Azimuth" on
key "Zenith_distance" on
key "Height_difference" on
key "GPS_baseline" on
key "Latitude" on
key "Longitude" on
key "Projection_bearing" on

key "Used_obs" on
key "Rejected_obs" on
key "Unused_obs" on


key "Map_background" WHITE
key "Text" BLACK
key "Highlight" GREEN
key "Selected" CORAL
