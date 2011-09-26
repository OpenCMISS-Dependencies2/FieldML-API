/* \file
 * $Id$
 * \author Caton Little
 * \brief 
 *
 * \section LICENSE
 *
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is FieldML
 *
 * The Initial Developer of the Original Code is Auckland Uniservices Ltd,
 * Auckland, New Zealand. Portions created by the Initial Developer are
 * Copyright (C) 2010 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 */

#include "fieldml_api.h"
#include "string_const.h"

#include "FieldmlErrorHandler.h"
#include "TextArrayDataReader.h"
#include "InputStream.h"

using namespace std;

TextArrayDataReader *TextArrayDataReader::create( FieldmlErrorHandler *eHandler, const char *root, TextArrayDataSource *source )
{
    FieldmlInputStream *stream = NULL;
    
    TextResource *resource = source->getResource();
    
    if( resource->type == DATA_RESOURCE_TEXT_HREF )
    {
        stream = FieldmlInputStream::createTextFileStream( makeFilename( root, resource->href ) );
    }
    else if( resource->type == DATA_RESOURCE_TEXT_INLINE )
    {
        //TODO This is unsafe, as the user can modify the string while the reader is still active.
        stream = FieldmlInputStream::createStringStream( resource->arrayString.c_str() );
    }
    
    if( stream == NULL )
    {
        return NULL;
    }
    
    return new TextArrayDataReader( stream, source, eHandler );
}


TextArrayDataReader::TextArrayDataReader( FieldmlInputStream *_stream, TextArrayDataSource *_source, FieldmlErrorHandler *_eHandler ) :
    ArrayDataReader( _eHandler ),
    stream( _stream ),
    source( _source )
{
    startPos = -1;
}


bool TextArrayDataReader::checkDimensions( int *offsets, int *sizes )
{
    for( int i = 0; i < source->rank; i++ )
    {
        if( offsets[i] < 0 )
        {
            return false;
        }
        if( sizes[i] <= 0 )
        {
            return false;
        }
        if( source->offsets[i] + offsets[i] + sizes[i] > source->sizes[i] )
        {
            return false;
        }
    }
    
    return true;
}


FmlErrorNumber TextArrayDataReader::skipPreamble()
{
    for( int i = 1; i < source->firstLine; i++ )
    {
        stream->skipLine();
    }
    
    if( stream->eof() )
    {
        return eHandler->setError( FML_ERR_IO_UNEXPECTED_EOF );
    }

    startPos = stream->tell();
    
    return FML_ERR_NO_ERROR;
}


bool TextArrayDataReader::applyOffsets( int *offsets, int depth )
{
    long count = 1;
    
    for( int i = 0; i < depth - 1; i++ )
    {
        //NOTE This could overflow in the event that someone puts that much data into a text file. Probability: Lilliputian.
        count *= source->textSizes[i];
    }
    
    for( int j = 0; j < source->offsets[depth-1] + offsets[depth-1]; j++ )
    {
        for( int i = 0; i < count; i++ )
        {
            stream->readDouble(); 
        }
    }
    
    return !stream->eof();
}


FmlErrorNumber TextArrayDataReader::readIntSlice( int *offsets, int *sizes, int *valueBuffer, int depth, int *bufferPos )
{
    if( !applyOffsets( offsets, depth ) )
    {
        return eHandler->setError( FML_ERR_IO_UNEXPECTED_EOF );
    }
    
    if( depth == 1 )
    {
        for( int i = 0; i < sizes[0]; i++ )
        {
            valueBuffer[*bufferPos] = stream->readInt();
            (*bufferPos)++;
        }
        if( stream->eof() )
        {
            return eHandler->setError( FML_ERR_IO_UNEXPECTED_EOF );
        }
        
        return FML_ERR_NO_ERROR;
    }

    int err;
    for( int i = 0; i < sizes[depth-1]; i++ )
    {
        err = readIntSlice( offsets, sizes, valueBuffer, depth - 1, bufferPos );
        if( err != FML_ERR_NO_ERROR )
        {
            return err;
        }
    }
    
    return FML_ERR_NO_ERROR;
}


FmlErrorNumber TextArrayDataReader::readIntSlab( int *offsets, int *sizes, int *valueBuffer )
{
    if( !checkDimensions( offsets, sizes ) )
    {
        return eHandler->setError( FML_ERR_INVALID_PARAMETERS );
    }
    
    if( startPos == -1 )
    {
        int err = skipPreamble();
        if( err != FML_ERR_NO_ERROR )
        {
            return err;
        }
    }
    else
    {
        stream->seek( startPos );
    }
    
    int bufferPos = 0;
    return readIntSlice( offsets, sizes, valueBuffer, source->rank, &bufferPos );
}


FmlErrorNumber TextArrayDataReader::readDoubleSlice( int *offsets, int *sizes, double *valueBuffer, int depth, int *bufferPos )
{
    if( !applyOffsets( offsets, depth ) )
    {
        return eHandler->setError( FML_ERR_IO_UNEXPECTED_EOF );
    }
    
    if( depth == 1 )
    {
        for( int i = 0; i < sizes[0]; i++ )
        {
            valueBuffer[*bufferPos] = stream->readDouble();
            (*bufferPos)++;
        }
        if( stream->eof() )
        {
            return eHandler->setError( FML_ERR_IO_UNEXPECTED_EOF );
        }
        
        return FML_ERR_NO_ERROR;
    }
    else
    {
        int err;
        for( int i = 0; i < sizes[depth-1]; i++ )
        {
            err = readDoubleSlice( offsets, sizes, valueBuffer, depth - 1, bufferPos );
            if( err != FML_ERR_NO_ERROR )
            {
                return err;
            }
        }
        
        return FML_ERR_NO_ERROR;
    }
}


FmlErrorNumber TextArrayDataReader::readDoubleSlab( int *offsets, int *sizes, double *valueBuffer )
{
    if( !checkDimensions( offsets, sizes ) )
    {
        return eHandler->setError( FML_ERR_INVALID_PARAMETERS );
    }
    
    if( startPos == -1 )
    {
        int err = skipPreamble();
        if( err != FML_ERR_NO_ERROR )
        {
            return err;
        }
    }
    else
    {
        stream->seek( startPos );
    }
    
    int bufferPos = 0;
    return readDoubleSlice( offsets, sizes, valueBuffer, source->rank, &bufferPos );
}


TextArrayDataReader::~TextArrayDataReader()
{
    delete stream;
}
