/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

////////////////////////////////////////////////////////////////////////////////////////
// RAVEN STANDARD TEMPLATE LIBRARY
//  (c) 2002 Activision
//
//
// String
// ------
// Simple wrapper around a char[SIZE] array.
//
//
//
// NOTES:
//
//
//
////////////////////////////////////////////////////////////////////////////////////////
#if !defined(RATL_STRING_VS_INC)
#define RATL_STRING_VS_INC

////////////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////////////
#if !defined(RATL_COMMON_INC)
#include "ratl_common.h"
#endif

namespace ratl
{
	////////////////////////////////////////////////////////////////////////////////////////
	// The String Class
	////////////////////////////////////////////////////////////////////////////////////////
	template <int ARG_CAPACITY>
	class string_vs : public ratl_base
	{
	public:
		////////////////////////////////////////////////////////////////////////////////////
		// Capacity Enum
		////////////////////////////////////////////////////////////////////////////////////
		static const int CAPACITY = ARG_CAPACITY;

	private:
		////////////////////////////////////////////////////////////////////////////////////
		// Data
		////////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
		char mData[CAPACITY + 4];
#else
		char	mData[CAPACITY];
#endif

		void FillTerminator()
		{
#ifdef _DEBUG
			mData[CAPACITY] = 'e';
			mData[CAPACITY + 1] = 'n';
			mData[CAPACITY + 2] = 'd';
			mData[CAPACITY + 3] = 0;
#endif
		}

	public:
		////////////////////////////////////////////////////////////////////////////////////
		// Constructor
		////////////////////////////////////////////////////////////////////////////////////
		string_vs()
		{
			mData[0] = 0;
			FillTerminator();
		}
#ifdef _DEBUG
		~string_vs()
		{
			//if you hit the below asserts, the end of the string was overwritten
			assert(mData[CAPACITY] == 'e');
			assert(mData[CAPACITY + 1] == 'n');
			assert(mData[CAPACITY + 2] == 'd');
			assert(mData[CAPACITY + 3] == 0);
		}
#endif
		////////////////////////////////////////////////////////////////////////////////////
		// Copy Constructor
		////////////////////////////////////////////////////////////////////////////////////
		string_vs(const string_vs<CAPACITY>& o)
		{
			assert(str::len(o.mData) < CAPACITY);
			str::ncpy(mData, o.mData, CAPACITY); // Safe String Copy
			mData[CAPACITY - 1] = 0; // Make Sure We Have A Null Terminated Str
			FillTerminator();
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Copy Constructor
		////////////////////////////////////////////////////////////////////////////////////
		string_vs(const char* s)
		{
			assert(str::len(s) < CAPACITY);
			str::ncpy(mData, s, CAPACITY); // Safe String Copy
			mData[CAPACITY - 1] = 0; // Make Sure We Have A Null Terminated Str
			FillTerminator();
		}

		////////////////////////////////////////////////////////////////////////////////////
		//
		////////////////////////////////////////////////////////////////////////////////////
		string_vs& operator=(const char* s)
		{
			assert(str::len(s) < CAPACITY);
			str::ncpy(mData, s, CAPACITY); // Safe String Copy
			mData[CAPACITY - 1] = 0; // Make Sure We Have A Null Terminated Str
			FillTerminator();
			return *this;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Access To Raw Array
		////////////////////////////////////////////////////////////////////////////////////
		char* c_str()
		{
			return mData;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Access To Raw Array
		////////////////////////////////////////////////////////////////////////////////////
		const char* c_str() const
		{
			return mData;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Access To Raw Array
		////////////////////////////////////////////////////////////////////////////////////
		operator const char* () const
		{
			return mData;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Access To Raw Array
		////////////////////////////////////////////////////////////////////////////////////
		const char* operator*() const
		{
			return mData;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// How Many Characters Can This Hold
		////////////////////////////////////////////////////////////////////////////////////
		static int capacity()
		{
			return CAPACITY;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Length
		////////////////////////////////////////////////////////////////////////////////////
		int length() const
		{
			assert(str::len(mData) < CAPACITY - 1);
			return str::len(mData);
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Character Bracket Operator
		////////////////////////////////////////////////////////////////////////////////////
		char operator[](int index)
		{
			assert(index < CAPACITY);
			return mData[index];
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Equality Operator
		////////////////////////////////////////////////////////////////////////////////////
		bool operator==(const string_vs& o) const
		{
			if (!stricmp(mData, o.mData))
			{
				return true;
			}
			return false;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// InEquality Operator
		////////////////////////////////////////////////////////////////////////////////////
		bool operator!=(const string_vs& o) const
		{
			if (str::icmp(mData, o.mData) != 0)
			{
				return true;
			}
			return false;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Compare Less Than
		////////////////////////////////////////////////////////////////////////////////////
		bool operator<(const string_vs& o) const
		{
			if (str::icmp(mData, o.mData) < 0)
			{
				return true;
			}
			return false;
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Compare Greater Than
		////////////////////////////////////////////////////////////////////////////////////
		bool operator>(const string_vs& o) const
		{
			if (str::icmp(mData, o.mData) > 0)
			{
				return true;
			}
			return false;
		}

		////////////////////////////////////////////////////////////////////////////////////
		//
		////////////////////////////////////////////////////////////////////////////////////
		void operator+=(const string_vs& o)
		{
			if (str::len(mData) + o.length() < CAPACITY) // Only If It Is Safe
			{
				str::cat(mData, o.c_str());
			}
			else
			{
				assert(!"string_vs overflow\n");
			}
		}

		////////////////////////////////////////////////////////////////////////////////////
		//
		////////////////////////////////////////////////////////////////////////////////////
		void operator+=(const char* s)
		{
			if (str::len(mData) + str::len(s) < CAPACITY) // Only If It Is Safe
			{
				str::cat(mData, s);
			}
			else
			{
				assert(!"string_vs overflow\n");
			}
		}

		////////////////////////////////////////////////////////////////////////////////////
		// Tokenizer
		//
		// The string tokenizer class is similar to an iterator in that it essentially
		// iterates over all the tokens in the string seperated by a common series of
		// delinating sequences.  For example:  " ,\t\n" would seperate tokens on spaces
		// commas, tabs and linefeeds.
		//
		// Iterating over string tokens is just like normal iteration:
		//
		// for (string_vs<CAPACITY>::tokenizer it=MyString.begin(" ,\t\n"); it!=MyString.end(); it++)
		// {
		//    const char* token = *it;
		// }
		//
		//
		// NOTE: This class is built upon the c library function strtok() which uses a
		// static working area, so having multiple tokenizers in multiple threads or just
		// plain at the same time is not safe.
		//
		////////////////////////////////////////////////////////////////////////////////////
		class tokenizer
		{
			enum
			{
				TOKEN_GAP_LEN = 15,
			};

		public:
			// Constructors
			//--------------
			tokenizer() : mLoc(nullptr), mGap{}
			{
			}

			tokenizer(const char* t, const char* gap)
			{
				strncpy(mGap, gap, TOKEN_GAP_LEN); // Safe String Copy
				mGap[TOKEN_GAP_LEN - 1] = 0; // Make Sure We Have A Null Terminated Str

				const auto temp = const_cast<char*>(t);
				mLoc = str::tok(temp, mGap);
			}

			// Assignment Operator
			//---------------------
			void operator=(const tokenizer& t)
			{
				mLoc = t.mLoc;
				str::cpy(mGap, t.mGap);
			}

			// Equality Operators
			//--------------------
			bool operator==(const tokenizer& t) { return mLoc == t.mLoc; }
			bool operator!=(const tokenizer& t) { return !operator==(t); }

			// DeReference Operator
			//----------------------
			const char* operator*() const
			{
				assert(mLoc);
				return mLoc;
			}

			// Inc & Dec Operators
			//--------------------
			void operator++(int)
			{
				assert(mLoc && mGap[0]);
				mLoc = str::tok(nullptr, mGap);
			}

			// Data
			//------
		private:
			char* mLoc;
			char mGap[TOKEN_GAP_LEN];
		};

		////////////////////////////////////////////////////////////////////////////////////
		// Get An Iterator To The First Token Seperated By Gap
		////////////////////////////////////////////////////////////////////////////////////
		tokenizer begin(const char* gap)
		{
			return tokenizer(mData, gap);
		}

		////////////////////////////////////////////////////////////////////////////////////
		// The Invalid Iterator, Use As A Stop Condition In Your For Loops
		////////////////////////////////////////////////////////////////////////////////////
		tokenizer end()
		{
			return tokenizer();
		}
	};
}

//fixme get rid of these
using TString_vs = ratl::string_vs<256>;
using TUIString_vs = ratl::string_vs<128>;

#endif
