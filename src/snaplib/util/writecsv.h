#ifndef _WRITECSV_H
#define _WRITECSV_H

typedef struct
{
    char *filename;
    FILE *f;
    char tab;
    char first;
    char delim;
    char *delimrep;
    char quote;
    char *quoterep;
    char *newlinerep;
    char charbuf[20];
} output_csv;


output_csv *open_output_csv( const char *filename, int tab_delimited );
void close_output_csv( output_csv *csv );
void end_output_csv_record( output_csv *csv );
void write_csv_header( output_csv *csv, const char *fieldname );
void write_csv_string( output_csv *csv, const char *value );
void write_csv_int( output_csv *csv, long value );
void write_csv_double( output_csv *csv, double value, int ndp );
void write_csv_date( output_csv *csv, double date );
void write_csv_null_field( output_csv *csv );

#define WRITECSV_CSV_EXT ".csv"
#define WRITECSV_TAB_EXT ".txt"

#endif
