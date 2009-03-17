/***************************************************************************\
 *
 *
 *            ___        ___           ___           ___
 *           /  /\      /  /\         /  /\         /  /\
 *          /  /:/     /  /::\       /  /::\       /  /::\
 *         /  /:/     /  /:/\:\     /  /:/\:\     /  /:/\:\
 *        /  /:/     /  /:/~/:/    /  /:/~/::\   /  /:/~/:/
 *       /  /::\    /__/:/ /:/___ /__/:/ /:/\:\ /__/:/ /:/
 *      /__/:/\:\   \  \:\/:::::/ \  \:\/:/__\/ \  \:\/:/
 *      \__\/  \:\   \  \::/~~~~   \  \::/       \  \::/
 *           \  \:\   \  \:\        \  \:\        \  \:\
 *            \  \ \   \  \:\        \  \:\        \  \:\
 *             \__\/    \__\/         \__\/         \__\/
 *
 *
 *
 *
 *   This file is part of TRAP.
 *
 *   TRAP is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *   or see <http://www.gnu.org/licenses/>.
 *
 *
 *
 *   (c) Luca Fossati, fossati@elet.polimi.it
 *
\***************************************************************************/

#ifndef CUSTOMEXCEPTION_HPP
#define CUSTOMEXCEPTION_HPP

#include <cstdlib>
#include <string>
#include <exception>
#include <stdexcept>
#include <execinfo.h>

class annull_exception: public std::runtime_error{
    public:
    annull_exception() : std::runtime_error(""){}
    annull_exception(const char * message) : std::runtime_error(message){}
};

#ifdef __GNUC__
class ExceptionTracer : public std::runtime_error{
  public:
    ExceptionTracer(std::string errorMex) : std::runtime_error(errorMex){
        void * array[25];
        int nSize = backtrace(array, 25);
        char ** symbols = backtrace_symbols(array, nSize);

        for (int i = 0; i < nSize; i++){
            std::cerr << symbols[i] << std::endl;
        }
        std::cerr << std::endl;
        free(symbols);
    }
};
#else
class ExceptionTracer : public std::runtime_error{
  public:
    ExceptionTracer(std::string errorMex) : std::runtime_error(errorMex){}
};
#endif

#endif
