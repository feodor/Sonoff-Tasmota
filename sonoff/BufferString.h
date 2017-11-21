/*
 WBufferString.h - BufferString library for Wiring & Arduino
 ...mostly rewritten by Paul Stoffregen...
 Copyright (c) 2009-10 Hernando Barragan.  All right reserved.
 Copyright 2011, Paul Stoffregen, paul@pjrc.com

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef BufferString_class_h
#define BufferString_class_h
#ifdef __cplusplus

#include <stdlib.h>
#include <string.h>
#include <WString.h>
#include <ctype.h>
#include <pgmspace.h>

// The string class
class BufferString {
		// use a function pointer to allow for "if (s)" without the
		// complications of an operator bool(). for more information, see:
		// http://www.artima.com/cppsource/safebool.html
		typedef void (BufferString::*BufferStringIfHelperType)() const;
		void BufferStringIfHelper() const {
		}

	public:
		// constructor uses buffer as storage
		BufferString(char *cstr, unsigned int clen);

		~BufferString(void);

		void reset(void);
		// memory management
		// return true on success, false on failure (in which case, the string
		// is left unchanged).  reserve(0), if successful, will validate an
		// invalid string (i.e., "if (s)" will be true afterwards)
		inline unsigned int length(void) const {
			if(buffer) {
				return len;
			} else {
				return 0;
			}
		}

		// creates a copy of the assigned value.  if the value is null or
		// invalid, or if the memory allocation fails, the string will be
		// marked as invalid ("if (s)" will be false).
		BufferString & operator =(const BufferString &rhs);
		BufferString & operator =(const char *cstr);
		BufferString & operator = (const __FlashStringHelper *str);

		// concatenate (works w/ built-in types)

		// returns true on success, false on failure (in which case, the string
		// is left unchanged).  if the argument is null or invalid, the
		// concatenation is considered unsucessful.
		unsigned char concat(const BufferString &str);
		unsigned char concat(const char *cstr);
		unsigned char concat(char c);
		unsigned char concat(unsigned char c);
		unsigned char concat(int num);
		unsigned char concat(unsigned int num);
		unsigned char concat(long num);
		unsigned char concat(unsigned long num);
		unsigned char concat(float num);
		unsigned char concat(double num);
		unsigned char concat(const __FlashStringHelper * str);

		// if there's not enough memory for the concatenated value, the string
		// will be left unchanged (but this isn't signalled in any way)
		BufferString & operator +=(const BufferString &rhs) {
			concat(rhs);
			return (*this);
		}
		BufferString & operator +=(const char *cstr) {
			concat(cstr);
			return (*this);
		}
		BufferString & operator +=(char c) {
			concat(c);
			return (*this);
		}
		BufferString & operator +=(unsigned char num) {
			concat(num);
			return (*this);
		}
		BufferString & operator +=(int num) {
			concat(num);
			return (*this);
		}
		BufferString & operator +=(unsigned int num) {
			concat(num);
			return (*this);
		}
		BufferString & operator +=(long num) {
			concat(num);
			return (*this);
		}
		BufferString & operator +=(unsigned long num) {
			concat(num);
			return (*this);
		}
		BufferString & operator +=(float num) {
			concat(num);
			return (*this);
		}
		BufferString & operator +=(double num) {
			concat(num);
			return (*this);
		}
		BufferString & operator += (const __FlashStringHelper *str){
			concat(str);
			return (*this);
		}

		// comparison (only works w/ BufferStrings and "strings")
		operator BufferStringIfHelperType() const {
			return buffer ? &BufferString::BufferStringIfHelper : 0;
		}
		int compareTo(const BufferString &s) const;
		unsigned char equals(const BufferString &s) const;
		unsigned char equals(const char *cstr) const;
		unsigned char operator ==(const BufferString &rhs) const {
			return equals(rhs);
		}
		unsigned char operator ==(const char *cstr) const {
			return equals(cstr);
		}
		unsigned char operator !=(const BufferString &rhs) const {
			return !equals(rhs);
		}
		unsigned char operator !=(const char *cstr) const {
			return !equals(cstr);
		}
		unsigned char operator <(const BufferString &rhs) const;
		unsigned char operator >(const BufferString &rhs) const;
		unsigned char operator <=(const BufferString &rhs) const;
		unsigned char operator >=(const BufferString &rhs) const;
		unsigned char equalsIgnoreCase(const BufferString &s) const;
		unsigned char startsWith(const BufferString &prefix) const;
		unsigned char startsWith(const BufferString &prefix, unsigned int offset) const;
		unsigned char endsWith(const BufferString &suffix) const;
		unsigned char endsWith(const char *suffix) const;
		unsigned char endsWith(const char suffix) const;

		// character acccess
		char charAt(unsigned int index) const;
		void setCharAt(unsigned int index, char c);
		char operator [](unsigned int index) const;
		char& operator [](unsigned int index);
		void getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index = 0) const;
		void toCharArray(char *buf, unsigned int bufsize, unsigned int index = 0) const {
			getBytes((unsigned char *) buf, bufsize, index);
		}
		const char * c_str() const {
			return buffer;
		}

		// search
		int indexOf(char ch) const;
		int indexOf(char ch, unsigned int fromIndex) const;
		int indexOf(const __FlashStringHelper *str) const;
		int indexOf(const __FlashStringHelper *str, unsigned int fromIndex) const;
		int indexOf(const BufferString &str) const;
		int indexOf(const BufferString &str, unsigned int fromIndex) const;
		int lastIndexOf(char ch) const;
		int lastIndexOf(char ch, unsigned int fromIndex) const;
		int lastIndexOf(const BufferString &str) const;
		int lastIndexOf(const BufferString &str, unsigned int fromIndex) const;
		int lastIndexOf(const __FlashStringHelper *str) const;
		int lastIndexOf(const __FlashStringHelper *str, unsigned int fromIndex) const;

		// modification
		void replace(char find, char replace);
		void replace(const char *find, unsigned int findlen,
					 const char *replace, unsigned int replacelen);
		void replace(const BufferString& find, const char *replace);
		void replace(const __FlashStringHelper *find, const char *replace);
		void remove(unsigned int index);
		void remove(unsigned int index, unsigned int count);
		void toLowerCase(void);
		void toUpperCase(void);
		void trim(void);

		// parsing/conversion
		long toInt(void) const;
		float toFloat(void) const;

	protected:
		char *buffer;			// the actual char array
		unsigned int capacity;  // the array length minus one (for the '\0')
		unsigned int len;	   // the BufferString length (not counting the '\0')
	protected:
		void invalidate(void);
		unsigned char changeBuffer(unsigned int maxStrLen);
		unsigned char concat(const char *cstr, unsigned int length);

		// copy and move
		BufferString & copy(const char *cstr, unsigned int length);
		BufferString & copy(const __FlashStringHelper *pstr, unsigned int length);
};

#endif  // __cplusplus
#endif  // BufferString_class_h
