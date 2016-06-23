#!/bin/sh
gridtool read nzgeoid2016.xyz write_linzgrid NZGD2000 "New Zealand Geoid 2016 (NZGeoid2016)" 'Geoid values computed on 1" by 1" grid' "Computed by Land Information New Zealand" resolution 0.001 nzgeoid2016.gdf
grid_convert nzgeoid2016.gdf nzgeoid2016.grd
concord -g nzgeoid2016.grd -i nzgd2000:enh:d -o nzgd2000:eno:d nzgeoid2016.xyz check.xyz
