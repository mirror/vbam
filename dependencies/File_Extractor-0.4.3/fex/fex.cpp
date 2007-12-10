// File_Extractor 0.4.3. http://www.slack.net/~ant/

#include "fex.h"

#include "File_Extractor.h"
#include "blargg_endian.h"
#include <string.h>
#include <ctype.h>

/* Copyright (C) 2005-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

#ifndef FEX_TYPE_LIST

// Archive types supported by default. If you want to change this list,
// modify the copy in blargg_config.h, NOT this one.
#if FEX_ENABLE_RAR
#define FEX_TYPE_LIST \
	fex_7z_type, fex_gz_type, fex_zip_type, fex_bin_type
#else
#define FEX_TYPE_LIST \
	fex_7z_type, fex_gz_type, fex_zip_type, fex_bin_type
#endif

#endif

static fex_type_t const fex_type_list_ [] = { FEX_TYPE_LIST, 0 };

fex_type_t const* fex_type_list() { return fex_type_list_; }

void fex_close( File_Extractor* fe ) { delete fe; }

void*     fex_user_data         ( File_Extractor const* fe )                    { return fe->user_data(); }
void      fex_set_user_data     ( File_Extractor* fe, void* new_user_data )     { fe->set_user_data( new_user_data ); }
void      fex_set_user_cleanup  ( File_Extractor* fe, fex_user_cleanup_t func ) { fe->set_user_cleanup( func ); }

fex_type_t fex_type             ( File_Extractor const* fe )                    { return fe->type(); }
int       fex_done              ( File_Extractor const* fe )                    { return fe->done(); }
const char* fex_name            ( File_Extractor const* fe )                    { return fe->name(); }
long      fex_size              ( File_Extractor const* fe )                    { return fe->size(); }
unsigned long fex_dos_date      ( File_Extractor const* fe )                    { return fe->dos_date(); }
long      fex_remain            ( File_Extractor const* fe )                    { return fe->remain(); }
fex_err_t fex_read              ( File_Extractor* fe, void* out, long count )   { return fe->read( out, count ); }
long      fex_read_avail        ( File_Extractor* fe, void* out, long count )   { return fe->read_avail( out, count ); }
fex_err_t fex_read_once         ( File_Extractor* fe, void* out, long count )   { return fe->read_once( out, count ); }
fex_err_t fex_next              ( File_Extractor* fe )                          { return fe->next(); }
fex_err_t fex_rewind            ( File_Extractor* fe )                          { return fe->rewind(); }
void      fex_scan_only         ( File_Extractor* fe )                          { fe->scan_only(); }

const unsigned char* fex_data( File_Extractor* fe, fex_err_t* err_out )
{
	blargg_err_t berr;
	const unsigned char* p = fe->data( &berr );
	if ( err_out )
		*err_out = berr;
	return p;
}

const char* fex_identify_header( void const* header )
{
	unsigned long four = get_be32( header );
	switch ( four )
	{
		case 0x52457E5E:
		case 0x52617221: return "RAR";

		case 0x377ABCAF: return "7Z";

		case 0x504B0304:
		case 0x504B0506: return "ZIP";

		// TODO: also identify other archive types *not* supported?
		// unsupported types, to avoid opening them in Single_File_Extractor
		//case 0x53495421: return "SIT";
	}

	if ( (four >> 16) == 0x1F8B )
		return "GZ";

	return "";
}

static void to_uppercase( const char* in, int len, char* out )
{
	for ( int i = 0; i < len; i++ )
	{
		if ( !(out [i] = toupper( in [i] )) )
			return;
	}
	*out = 0; // extension too long
}

fex_type_t fex_identify_extension( const char* extension_ )
{
	char const* end = strrchr( extension_, '.' );
	if ( end )
		extension_ = end + 1;

	char extension [6];
	to_uppercase( extension_, sizeof extension, extension );

	fex_type_t result = 0;
	for ( fex_type_t const* types = fex_type_list_; *types; types++ )
	{
		if ( !*(*types)->extension )
			result = *types; // fex_bin_type, so use if nothing else matches

		if ( !strcmp( extension, (*types)->extension ) )
			return *types;
	}
	return result;
}

static fex_err_t fex_identify_file_( const char* path, fex_type_t* type_out )
{
	*type_out = fex_identify_extension( path );
	if ( !*type_out || !strrchr( path, '.' ) )
	{
		*type_out = 0;

		FEX_FILE_READER in;
		RETURN_ERR( in.open( path ) );

		char header [4] = { 0 };
		if ( in.read_avail( header, sizeof header ) < 0 )
			return "Couldn't read file";

		*type_out = fex_identify_extension( fex_identify_header( header ) );
	}
	return 0;
}

fex_type_t fex_identify_file( const char* path, fex_err_t* err_out )
{
	fex_type_t type;
	fex_err_t err = fex_identify_file_( path, &type );
	if ( err_out )
		*err_out = err;
	return type;
}

static fex_err_t fex_open_type_( fex_type_t type, const char* path, File_Extractor** fe_out )
{
	*fe_out = 0;
	if ( !type )
		return fex_wrong_file_type;

	File_Extractor* fe = type->new_fex();
	CHECK_ALLOC( fe );

	fex_err_t err = fe->open( path );
	if ( err )
	{
		delete fe;
		return err;
	}

	*fe_out = fe;
	return 0;
}

File_Extractor* fex_open_type( fex_type_t type, const char* path, fex_err_t* err_out )
{
	File_Extractor* fe;
	fex_err_t err = fex_open_type_( type, path, &fe );
	if ( err_out )
		*err_out = err;
	return fe;
}

File_Extractor* fex_open( const char* path, fex_err_t* err_out )
{
	fex_type_t type = fex_identify_file( path, err_out );
	if ( !type )
		return 0;

	return fex_open_type( type, path, err_out );
}
