/*
 WBufferString.cpp - BufferString library for Wiring & Arduino
 ...mostly rewritten by Paul Stoffregen...
 Copyright (c) 2009-10 Hernando Barragan.  All rights reserved.
 Copyright 2011, Paul Stoffregen, paul@pjrc.com
 Modified by Ivan Grokhotkov, 2014 - esp8266 support
 Modified by Michael C. Miller, 2015 - esp8266 progmem support

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

#include <Arduino.h>
#include "BufferString.h"
#include "stdlib_noniso.h"

/*********************************************/
/*  Constructors							 */
/*********************************************/

BufferString::BufferString(char *cstr, unsigned int clen) {
	buffer = cstr;
	*buffer = '\0';
	capacity = clen;
	len = 0;
}

BufferString::~BufferString() {
	invalidate();
}

void BufferString::invalidate(void) {
	buffer = NULL;
	capacity = 0;
	len = 0;
}

void BufferString::reset(void) {
	len = 0;
	*buffer = '\0';
}

// /*********************************************/
// /*  Copy and Move							*/
// /*********************************************/

BufferString & BufferString::copy(const char *cstr, unsigned int length) {
	if (length >= capacity) {
		invalidate();
		return *this;
	}
	len = length;
	strcpy(buffer, cstr);
	return *this;
}

BufferString & BufferString::copy(const __FlashStringHelper *pstr, unsigned int length) {
	if (length >= capacity) {
		invalidate();
		return *this;
	}
	len = length;
	strcpy_P(buffer, (PGM_P)pstr);
	return *this;
}

BufferString & BufferString::operator =(const BufferString &rhs) {
	if(this == &rhs)
		return *this;

	copy(rhs.buffer, rhs.len);

	return *this;
}

BufferString & BufferString::operator =(const char *cstr) {
	if(cstr)
		copy(cstr, strlen(cstr));
	else
		invalidate();

	return *this;
}

BufferString & BufferString::operator = (const __FlashStringHelper *pstr)
{
	if (pstr) copy(pstr, strlen_P((PGM_P)pstr));
	else invalidate();

	return *this;
}

// /*********************************************/
// /*  concat								   */
// /*********************************************/

unsigned char BufferString::concat(const BufferString &s) {
	return concat(s.buffer, s.len);
}

unsigned char BufferString::concat(const char *cstr, unsigned int length) {
	unsigned int newlen = len + length;
	if(!cstr)
		return 0;
	if(length == 0)
		return 1;
	if(newlen >= capacity)
		return 0;
	strcpy(buffer + len, cstr);
	len = newlen;
	return 1;
}

unsigned char BufferString::concat(const char *cstr) {
	if(!cstr)
		return 0;
	return concat(cstr, strlen(cstr));
}

unsigned char BufferString::concat(char c) {
	char buf[2];
	buf[0] = c;
	buf[1] = 0;
	return concat(buf, 1);
}

unsigned char BufferString::concat(unsigned char num) {
	char buf[1 + 3 * sizeof(unsigned char)];
	itoa(num, buf, 10);
	return concat(buf, strlen(buf));
}

unsigned char BufferString::concat(int num) {
	char buf[2 + 3 * sizeof(int)];
	itoa(num, buf, 10);
	return concat(buf, strlen(buf));
}

unsigned char BufferString::concat(unsigned int num) {
	char buf[1 + 3 * sizeof(unsigned int)];
	utoa(num, buf, 10);
	return concat(buf, strlen(buf));
}

unsigned char BufferString::concat(long num) {
	char buf[2 + 3 * sizeof(long)];
	ltoa(num, buf, 10);
	return concat(buf, strlen(buf));
}

unsigned char BufferString::concat(unsigned long num) {
	char buf[1 + 3 * sizeof(unsigned long)];
	ultoa(num, buf, 10);
	return concat(buf, strlen(buf));
}

unsigned char BufferString::concat(float num) {
	char buf[20];
	char* string = dtostrf(num, 4, 2, buf);
	return concat(string, strlen(string));
}

unsigned char BufferString::concat(double num) {
	char buf[20];
	char* string = dtostrf(num, 4, 2, buf);
	return concat(string, strlen(string));
}

unsigned char BufferString::concat(const __FlashStringHelper * str) {
	if (!str) return 0;
	int length = strlen_P((PGM_P)str);
	if (length == 0) return 1;
	unsigned int newlen = len + length;
	if (newlen >= capacity) return 0;
	strncpy_P(buffer + len, (PGM_P)str, length);
	len = newlen;
	buffer[len] = '\0';
	return 1;
}

// /*********************************************/
// /*  Comparison							   */
// /*********************************************/

int BufferString::compareTo(const BufferString &s) const {
	if(!len || !s.len) {
		if(s.buffer && s.len > 0)
			return 0 - *(unsigned char *) s.buffer;
		if(buffer && len > 0)
			return *(unsigned char *) buffer;
		return 0;
	}
	return strcmp(buffer, s.buffer);
}

unsigned char BufferString::equals(const BufferString &s2) const {
	return (len == s2.len && compareTo(s2) == 0);
}

unsigned char BufferString::equals(const char *cstr) const {
	if(len == 0)
		return (cstr == NULL || *cstr == 0);
	if(cstr == NULL)
		return buffer[0] == 0;
	return strcmp(buffer, cstr) == 0;
}

unsigned char BufferString::operator<(const BufferString &rhs) const {
	return compareTo(rhs) < 0;
}

unsigned char BufferString::operator>(const BufferString &rhs) const {
	return compareTo(rhs) > 0;
}

unsigned char BufferString::operator<=(const BufferString &rhs) const {
	return compareTo(rhs) <= 0;
}

unsigned char BufferString::operator>=(const BufferString &rhs) const {
	return compareTo(rhs) >= 0;
}

unsigned char BufferString::equalsIgnoreCase(const BufferString &s2) const {
	if(this == &s2)
		return 1;
	if(len != s2.len)
		return 0;
	if(len == 0)
		return 1;
	const char *p1 = buffer;
	const char *p2 = s2.buffer;
	while(*p1) {
		if(tolower(*p1++) != tolower(*p2++))
			return 0;
	}
	return 1;
}

unsigned char BufferString::startsWith(const BufferString &s2) const {
	if(len < s2.len)
		return 0;
	return startsWith(s2, 0);
}

unsigned char BufferString::startsWith(const BufferString &s2, unsigned int offset) const {
	if(offset > len - s2.len || !buffer || !s2.buffer)
		return 0;
	return strncmp(&buffer[offset], s2.buffer, s2.len) == 0;
}

unsigned char BufferString::endsWith(const BufferString &s2) const {
	if(len < s2.len || !buffer || !s2.buffer)
		return 0;
	return strcmp(&buffer[len - s2.len], s2.buffer) == 0;
}

unsigned char BufferString::endsWith(const char * suffix) const {
	int l = suffix ? strlen(suffix) : 0;

	if(len < l || !buffer || !suffix)
		return 0;
	return strcmp(&buffer[len - l], suffix) == 0;
}

unsigned char BufferString::endsWith(const char suffix) const {
	if (len == 0)
		return 0;

	return buffer[len - 1] == suffix;
}

// /*********************************************/
// /*  Character Access						 */
// /*********************************************/

char BufferString::charAt(unsigned int loc) const {
	return operator[](loc);
}

void BufferString::setCharAt(unsigned int loc, char c) {
	if(loc < len)
		buffer[loc] = c;
}

char & BufferString::operator[](unsigned int index) {
	static char dummy_writable_char;
	if(index >= len || !buffer) {
		dummy_writable_char = 0;
		return dummy_writable_char;
	}
	return buffer[index];
}

char BufferString::operator[](unsigned int index) const {
	if(index >= len || !buffer)
		return 0;
	return buffer[index];
}

void BufferString::getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index) const {
	if(!bufsize || !buf)
		return;
	if(index >= len) {
		buf[0] = 0;
		return;
	}
	unsigned int n = bufsize - 1;
	if(n > len - index)
		n = len - index;
	strncpy((char *) buf, buffer + index, n);
	buf[n] = 0;
}

// /*********************************************/
// /*  Search								   */
// /*********************************************/

int BufferString::indexOf(char c) const {
	return indexOf(c, 0);
}

int BufferString::indexOf(const __FlashStringHelper *str) const {
	 return indexOf(str, 0);	
}

int BufferString::indexOf(const __FlashStringHelper *str, unsigned int fromIndex) const {
	if(fromIndex >= len)
		return -1;
	const char *found = strstr_P(buffer + fromIndex, (PGM_P)str);
	if(found == NULL)
		return -1;
	return found - buffer;
}

int BufferString::indexOf(char ch, unsigned int fromIndex) const {
	if(fromIndex >= len)
		return -1;
	const char* temp = strchr(buffer + fromIndex, ch);
	if(temp == NULL)
		return -1;
	return temp - buffer;
}

int BufferString::indexOf(const BufferString &s2) const {
	return indexOf(s2, 0);
}

int BufferString::indexOf(const BufferString &s2, unsigned int fromIndex) const {
	if(fromIndex >= len)
		return -1;
	const char *found = strstr(buffer + fromIndex, s2.buffer);
	if(found == NULL)
		return -1;
	return found - buffer;
}

int BufferString::lastIndexOf(char theChar) const {
	return lastIndexOf(theChar, len - 1);
}

int BufferString::lastIndexOf(char ch, unsigned int fromIndex) const {
	if(fromIndex >= len)
		return -1;
	char tempchar = buffer[fromIndex + 1];
	buffer[fromIndex + 1] = '\0';
	char* temp = strrchr(buffer, ch);
	buffer[fromIndex + 1] = tempchar;
	if(temp == NULL)
		return -1;
	return temp - buffer;
}

int BufferString::lastIndexOf(const BufferString &s2) const {
	return lastIndexOf(s2, len - s2.len);
}

int BufferString::lastIndexOf(const BufferString &s2, unsigned int fromIndex) const {
	if(s2.len == 0 || len == 0 || s2.len > len)
		return -1;
	if(fromIndex >= len)
		fromIndex = len - 1;
	int found = -1;
	for(char *p = buffer; p <= buffer + fromIndex; p++) {
		p = strstr(p, s2.buffer);
		if(!p)
			break;
		if((unsigned int) (p - buffer) <= fromIndex)
			found = p - buffer;
	}
	return found;
}

int BufferString::lastIndexOf(const __FlashStringHelper *str) const {
	return lastIndexOf(str, len - strlen_P((PGM_P)str));
}

int BufferString::lastIndexOf(const __FlashStringHelper *str, unsigned int fromIndex) const {
	unsigned int strlen = strlen_P((PGM_P)str);

	if(strlen == 0 || len == 0 || strlen > len)
		return -1;
	if(fromIndex >= len)
		fromIndex = len - 1;
	int found = -1;
	for(char *p = buffer; p <= buffer + fromIndex; p++) {
		p = strstr_P(p, (PGM_P)str);
		if(!p)
			break;
		if((unsigned int) (p - buffer) <= fromIndex)
			found = p - buffer;
	}
	return found;
}

// /*********************************************/
// /*  Modification							 */
// /*********************************************/

void BufferString::replace(char find, char replace) {
	if(!buffer)
		return;
	for(char *p = buffer; *p && p - buffer < len; p++) {
		if(*p == find)
			*p = replace;
	}
}

void BufferString::replace(const char *find, unsigned int findlen,
						  const char *replace, unsigned int replacelen) {
	if(len == 0 || findlen == 0)
		return;
	int diff = replacelen - findlen;
	char *readFrom = buffer;
	char *foundAt;
	if(diff == 0) {
		while((foundAt = strstr(readFrom, find)) != NULL) {
			memcpy(foundAt, replace, replacelen);
			readFrom = foundAt + replacelen;
		}
	} else if(diff < 0) {
		char *writeTo = buffer;
		while((foundAt = strstr(readFrom, find)) != NULL) {
			unsigned int n = foundAt - readFrom;
			memcpy(writeTo, readFrom, n);
			writeTo += n;
			memcpy(writeTo, replace, replacelen);
			writeTo += replacelen;
			readFrom = foundAt + findlen;
			len += diff;
		}
		strcpy(writeTo, readFrom);
	} else {
		unsigned int size = len; // compute size needed for result
		while((foundAt = strstr(readFrom, find)) != NULL) {
			readFrom = foundAt + findlen;
			size += diff;
		}
		if(size == len)
			return;
		if(size >= capacity)
			return; // XXX: tell user!
		int index = len - 1;
		while(index >= 0 && (index = lastIndexOf(BufferString((char*)find, findlen), index)) >= 0) {
			readFrom = buffer + index + findlen;
			memmove(readFrom + diff, readFrom, len - (readFrom - buffer));
			len += diff;
			buffer[len] = 0;
			memcpy(buffer + index, replace, replacelen);
			index--;
		}
	}
}

void BufferString::replace(const BufferString& find, const char *repl) {
	replace(find.buffer, find.len, repl, strlen(repl));
}

void BufferString::replace(const __FlashStringHelper *find,
						  const char *replace) {
	unsigned int findlen = strlen_P((PGM_P)find);
	if(len == 0 || findlen == 0)
		return;
	unsigned int replacelen = strlen(replace);
	int diff = replacelen - findlen;
	char *readFrom = buffer;
	char *foundAt;
	if(diff == 0) {
		while((foundAt = strstr_P(readFrom, (PGM_P)find)) != NULL) {
			memcpy(foundAt, replace, replacelen);
			readFrom = foundAt + replacelen;
		}
	} else if(diff < 0) {
		char *writeTo = buffer;
		while((foundAt = strstr_P(readFrom, (PGM_P)find)) != NULL) {
			unsigned int n = foundAt - readFrom;
			memcpy(writeTo, readFrom, n);
			writeTo += n;
			memcpy(writeTo, replace, replacelen);
			writeTo += replacelen;
			readFrom = foundAt + findlen;
			len += diff;
		}
		strcpy(writeTo, readFrom);
	} else {
		unsigned int size = len; // compute size needed for result
		while((foundAt = strstr_P(readFrom, (PGM_P)find)) != NULL) {
			readFrom = foundAt + findlen;
			size += diff;
		}
		if(size == len)
			return;
		if(size >= capacity)
			return; // XXX: tell user!
		int index = len - 1;
		while(index >= 0 && (index = lastIndexOf(find, index)) >= 0) {
			readFrom = buffer + index + findlen;
			memmove(readFrom + diff, readFrom, len - (readFrom - buffer));
			len += diff;
			buffer[len] = 0;
			memcpy(buffer + index, replace, replacelen);
			index--;
		}
	}
}

void BufferString::remove(unsigned int index) {
	// Pass the biggest integer as the count. The remove method
	// below will take care of truncating it at the end of the
	// string.
	remove(index, (unsigned int) -1);
}

void BufferString::remove(unsigned int index, unsigned int count) {
	if(index >= len) {
		return;
	}
	if(count <= 0) {
		return;
	}
	if(count > len - index) {
		count = len - index;
	}
	char *writeTo = buffer + index;
	len = len - count;
	strncpy(writeTo, buffer + index + count, len - index);
	buffer[len] = 0;
}

void BufferString::toLowerCase(void) {
	if(!buffer)
		return;
	for(char *p = buffer; *p; p++) {
		*p = tolower(*p);
	}
}

void BufferString::toUpperCase(void) {
	if(!buffer)
		return;
	for(char *p = buffer; *p; p++) {
		*p = toupper(*p);
	}
}

void BufferString::trim(void) {
	if(!buffer || len == 0)
		return;
	char *begin = buffer;
	while(isspace(*begin))
		begin++;
	char *end = buffer + len - 1;
	while(isspace(*end) && end >= begin)
		end--;
	len = end + 1 - begin;
	if(begin > buffer)
		memcpy(buffer, begin, len);
	buffer[len] = 0;
}

// /*********************************************/
// /*  Parsing / Conversion					 */
// /*********************************************/

long BufferString::toInt(void) const {
	if(buffer)
		return atol(buffer);
	return 0;
}

float BufferString::toFloat(void) const {
	if(buffer)
		return atof(buffer);
	return 0;
}

int BufferString::sprintf(const char * format, ...) {
    int ret;
    va_list arglist;
    va_start(arglist, format);

    ret = vsprintf(format, arglist);

    va_end(arglist);

    return ret;
}

int BufferString::vsprintf(const char * format, va_list ap) {
    int ret;

    ret = vsnprintf(buffer + len, capacity - len, format, ap);

	if (ret + len >= capacity) {
		len = capacity - 1;
		buffer[len] = '\0';
	} else
		len += ret;

    return ret;
}

int BufferString::sprintf_P(const __FlashStringHelper * formatP, ...) {
    int ret;
    va_list arglist;
    va_start(arglist, formatP);

    ret = vsnprintf_P(buffer + len, capacity - len, (PGM_P)formatP, arglist);

    va_end(arglist);

	if (ret + len >= capacity) {
		len = capacity - 1;
		buffer[len] = '\0';
	} else
		len += ret;

    return ret;
}


