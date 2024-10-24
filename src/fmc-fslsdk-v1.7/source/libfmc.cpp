/* =====================================================================
 *
 * The MIT License (MIT)
 * Copyright 2009-2012, Freescale Semiconductor, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * File Name : libfmc.cpp
 * Author    : Serge Lamikhov-Center
 *
 * ===================================================================*/

#include <typeinfo>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <sstream>

#include "fmc.h"
#include "FMCGenericError.h"
#include "FMCTaskDef.h"
#include "FMCPDLReader.h"
#include "FMCPCDReader.h"
#include "FMCCFGReader.h"
#include "FMCPCDModel.h"
#include "FMCCModelOutput.h"
#include "FMCSP.h"

#define DEFINE_LOGGER_INSTANCE
#include "logger.hpp"


#ifdef _MSC_VER
#pragma warning(disable : 4996)
#ifdef _DEBUG
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.VC90.CRT' version='9.0.21022.8' processorArchitecture='x86' publicKeyToken='1fc8b3b9a1e18e3b' language='*'\"")
#endif
#endif


std::string error_text;
std::string compile_dump;

#ifdef __cplusplus
extern "C" {
#endif


int
fmc_compile(
    fmc_model*   cmodel,
    const char*  nameCfg,
    const char*  namePCD,
    const char*  namePDL,
    const char*  nameSP,
    unsigned int swOffset,
    unsigned int dontWarn,
    const char** dump
)
{
    error_text   = "";
    compile_dump = "";

    if ( cmodel == 0 ) {
        return 2;
    }
    
    memset( cmodel, 0, sizeof( *cmodel ) );

    try {
        if ( nameSP != 0 && std::string( nameSP ) == "nodefault" ) {
            nameSP = 0;
        }
        if ( namePCD != 0 && std::string( namePCD ) == "nodefault" ) {
            namePCD = 0;
        }
        if ( nameCfg != 0 && std::string( nameCfg ) == "nodefault" ) {
            nameCfg = 0;
        }

        CGenericError::dontWarn = dontWarn;

        CTaskDef task;

        // Process PDL and SP files
        CPDLReader pdlReader;
        pdlReader.setTaskData( &task );
        pdlReader.parseNetPDL( namePDL );

        // Process custom protocols definition - software parser portion
        if ( nameSP ) {
            pdlReader.setSoftParse(1);
            pdlReader.parseNetPDL( nameSP );
            softparser(&task, nameSP, swOffset);
        }

        if ( namePCD && nameCfg )
        {
            // Process PCD file
            CPCDReader pcdReader;
            pcdReader.setTaskData( &task );
            pcdReader.parseNetPCD( namePCD );

            // Process config file
            CCFGReader cfgReader;
            cfgReader.setTaskData( &task );
            cfgReader.parseCfgData( nameCfg );

            task.checkSemantics();

            CFMCModel model;
            LOG( logger::DBG1 ) << logger::ind( 2 )
                                << "Converting parsed task definition to internal representation ..."
                                << std::endl;
            model.createModel( &task );
            LOG( logger::DBG1 ) << logger::ind( -2 )
                                << "Done"
                                << std::endl;
            LOG( logger::DBG1 ) << std::endl;

            CFMCCModelOutput   modelOut;
            std::ostringstream oss;
            LOG( logger::DBG1 ) << logger::ind( 2 )
                                << "Output structures' initialization started ..."
                                << std::endl;
            modelOut.output( model, cmodel, oss, 0 );
            LOG( logger::DBG1 ) << logger::ind( -2 )
                                << "Done"
                                << std::endl;
            LOG( logger::DBG1 ) << std::endl;

            compile_dump = oss.str();
        }

        LOG( logger::DBG2 ) << "Generated init:"
                            << std::endl
                            << compile_dump
                            << std::endl;
        
        if ( dump != 0 ) {
            *dump = compile_dump.c_str();
        }
    }
    catch ( const CGenericError& genError ) { // Catch known errors generated by FMC
        error_text = genError.getErrorMsg();
        LOG( logger::ERR ) << error_text << std::endl;
        return 3;
    }
    catch ( std::exception &e )
    {
        error_text  = e.what();
        error_text += " Type : ";
        error_text += typeid( e ).name();
        LOG( logger::ERR ) << error_text << std::endl;
        return 4;
    }
    catch ( ... ) {                     // Catch unknown errors
        error_text = "Unrecognized error occured. The program was terminated";
        LOG( logger::ERR ) << error_text << std::endl;
        return 1;
    }

    return 0;
}


const char*
fmc_get_error( void )
{
    return error_text.c_str();
}


int32_t
fmc_log( int32_t level )
{
    using namespace logger;
    
    int32_t ret = LOG_GET_LEVEL();

    LOG_INIT( std::cerr, (log_level_t)level );

    return ret;
}


void
fmc_log_write( int32_t level, const char* format, ... )
{
    using namespace logger;

    va_list args;
    va_start( args, format );

    if ( level <= LOG_GET_LEVEL() ) {
        char buffer[256];
        vsprintf( buffer, format, args );
        LOG( (log_level_t)level ) << (const char*)buffer << std::endl;
    }

    va_end( args );
}


#ifdef __cplusplus
} // extern "C"
#endif

