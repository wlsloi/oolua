#ifndef OOLUA_USERDATA_H_
#	define OOLUA_USERDATA_H_

#include <cstring>
namespace OOLUA
{
    namespace INTERNAL
    {
        struct Lua_ud
        {
			void* void_class_ptr;
			//int is_const;
			char* name;
			char* none_const_name;//none constant name of the class
			int name_size;
            bool gc;//should it be garbage collected
        };
		inline bool id_is_const(Lua_ud* ud)
		{
			return ud->name != ud->none_const_name;
		}
#ifdef UNSAFE_ID_COMPARE
		inline bool ids_equal(char* lhsName,int /*lhsSize*/,char* rhsName,int /*rhsSize*/)
		{
			return lhsName == rhsName;
		}
#else
		inline bool ids_equal(char* lhsName,int lhsSize,char* rhsName,int rhsSize)
		{
			if(lhsSize != rhsSize)return false;
			return memcmp(lhsName,rhsName,lhsSize) == 0;
		}
#endif
    }
}
#endif
