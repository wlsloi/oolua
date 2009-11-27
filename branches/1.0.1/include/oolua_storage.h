#ifndef OOLUA_STORAGE_H_
#	define OOLUA_STORAGE_H_

#include "lua_includes.h"
#include "proxy_class.h"
#include "oolua_userdata.h"
#include "param_traits.h"
#include "lvd_types.h"
#include "base_checker.h"
#include "proxy_from_stack.h"
#include "type_list.h"
#include "oolua_char_arrays.h"
#include <cassert>

namespace OOLUA
{
	namespace INTERNAL
	{
		//extern char weak_lookup_name [];

		//pushes the weak top and returns its index
		int push_weak_table(lua_State* l);
		template<typename T>Lua_ud* add_ptr(lua_State* /*const*/ l,T* const ptr,bool isConst);

		//Lua_ud* find_ud(lua_State* /*const*/ l,void* ptr);
		template<typename T>Lua_ud* find_ud(lua_State* /*const*/ l,T* ptr,bool is_const);
		//userdata is top of stack and is still on top when it leaves
		void typed_delete_for_proxy_ptr(lua_State* l);
		bool is_there_an_entry_for_this_void_pointer(lua_State* l,void* ptr);
		bool is_there_an_entry_for_this_void_pointer(lua_State* l,void* ptr,int tableIndex);

		template<typename T>
		Lua_ud* replace_proxy_and_reset_metatable(lua_State* /*const*/ l,T* ptr,bool use_const_name);

		Lua_ud* find_ud_dont_care_about_type_and_clean_stack(lua_State* /*const*/ l,void* ptr);

		template<typename T>
		inline void remove_proxy_ptr(lua_State* /*const*/ l,T * ptr);

		void set_owner( lua_State* l,void* ptr, Owner own);

		void remove_classes_ud_if_required(lua_State* l,void* ptr, int tableIndex);

		void add_ptr_if_required(lua_State* const l, void* ptr,int udIndex,int weakIndex);

		template<typename Type,typename Bases, int BaseIndex,typename BaseType>
		struct Add_ptr;

		template<typename Type,typename Bases, int BaseIndex,typename BaseType>
		struct Set_nill_for_corrisponding_ud;

		template<typename Type,typename Bases, int BaseIndex,typename BaseType>
		struct Has_a_root_entry;

		template<typename T>
		int lua_set_owner(lua_State*  /*const*/ l);

		bool ud_at_index_is_const(lua_State* l, int index);

		template<typename T>
		int lua_set_owner(lua_State*  /*const*/ l)
		{
			Proxy_class<T>* p = proxy_from_index<T>(l,1);
			lua_remove(l,1);
			Owner own;
			pull2cpp(l,own);
			set_owner(l,p->m_this,own);
			return 0;
		}


		//It is possible for a base class and a derived class pointer to have no offset.
		//if found it is left on the top of the stack and returns the Lua_ud ptr
		//else the stack is same as on entrance to the function and null is returned
		template<typename T>
		inline Lua_ud* find_ud(lua_State* /*const*/ l,T* ptr,bool is_const)
		{
			bool has_entry = is_there_an_entry_for_this_void_pointer(l,ptr);//(ud or no addition to the stack)
			Lua_ud* ud(0);
			if(has_entry )//ud
			{
				/*
				possibilities:
					top of stack is the representation of the T ptr
					top of stack is derived from T with no offset pointer and it can be upcast to T
					top of stack is a registered base class of T with no offset pointer
				*/
				bool was_const = ud_at_index_is_const(l,-1);

				if( (was_const && is_const) || (is_const) )//no change required
				{
					if( proxy_from_stack_top<T>(l) )
						return static_cast<Lua_ud *>( lua_touserdata(l, -1) );
				}
				else if( was_const && !is_const)//change
				{
					if( proxy_from_stack_top<T>(l) )
					{
						lua_getmetatable(l,-1);//ud mt
						//lua_pushliteral(l,"__change_mt_to_none_const");//ud mt str
						push_char_carray(l,change_mt_to_none_const_field);//ud mt str
						lua_gettable(l,-2);//ud mt func
						lua_CFunction set_metatable_none_const = lua_tocfunction(l,-1);
						lua_pop(l,2);//ud
						set_metatable_none_const(l);
						return static_cast<Lua_ud *>( lua_touserdata(l, -1) );
					}
				}
				else //was not const and is not const
				{
					if( none_const_proxy_from_stack_top<T>(l) )
						return static_cast<Lua_ud *>( lua_touserdata(l, -1) );
				}

				//if T was a base of the stack or T was the stack it has been returned
				//top of stack is a registered base class of T with no offset pointer
				return replace_proxy_and_reset_metatable(l,ptr, was_const && is_const);
			}
			else
			{
				/*
				possibilities:
					a base class is stored.
					none of the hierarchy is stored
				*/

				int weak_table = push_weak_table(l);
				bool base_is_stored(false);
				Has_a_root_entry<
						T
						,typename FindRootBases<T>::Result
						,0
						,typename TYPELIST::At_default< typename FindRootBases<T>::Result, 0, TYPE::Null_type >::Result
					> checkRoots;
				checkRoots(l,ptr,weak_table,base_is_stored);
				lua_remove(l,weak_table);
				if(base_is_stored)
				{
					bool was_const = ud_at_index_is_const(l,-1);
					ud = replace_proxy_and_reset_metatable(l,ptr,was_const && is_const);
				}
			}
			return ud;
		}

		template<typename T>
		inline Lua_ud* replace_proxy_and_reset_metatable(lua_State* l,T* ptr,bool use_const_name)
		{
			typed_delete_for_proxy_ptr(l);
			//set to a newly created pointer of type Proxy_class<T>
			Lua_ud *ud = static_cast<Lua_ud *>( lua_touserdata(l, -1) );//ud
			ud->void_proxy_ptr = new Proxy_class<T>(ptr);
			//change the metatable associated with the ud
			if(use_const_name)
				lua_getfield(l, LUA_REGISTRYINDEX,OOLUA::Proxy_class<T>::class_name_const);
			else
				lua_getfield(l, LUA_REGISTRYINDEX,OOLUA::Proxy_class<T>::class_name);

			lua_setmetatable(l,-2);//set ud's metatable to this

			int weak_index = push_weak_table(l);//ud weakTable
			//then register all the bases that need it
			Add_ptr<T
					,typename OOLUA::Proxy_class<T>::AllBases
					,0
					,typename TYPELIST::At_default< typename OOLUA::Proxy_class<T>::AllBases, 0, TYPE::Null_type >::Result
				> addThisTypesBases;
			addThisTypesBases(l,ptr,weak_index-1,weak_index);
			lua_pop(l,1);//ud
			return ud;
		}

		template<typename T>
		inline Lua_ud* add_ptr(lua_State* const l,T* const ptr,bool isConst)
		{
			Lua_ud* ud = reinterpret_cast<Lua_ud*>(lua_newuserdata(l, sizeof(Lua_ud)));
			ud->void_proxy_ptr = new Proxy_class<T>(ptr);
			ud->gc = false;
			if(isConst)
				luaL_getmetatable(l, Proxy_class<T>::class_name_const);
			else
				luaL_getmetatable(l, Proxy_class<T>::class_name);  // lookup metatable in Lua registry


			assert( lua_isnoneornil(l,-1) ==0 && "no metatable of this name found in registry" );
			////Pops a table from the stack and sets it as the new metatable for the value at the given acceptable index
			lua_setmetatable(l, -2);

			int udIndex = lua_gettop(l);
			int weakIndex = push_weak_table(l);//ud,weakTable

			add_ptr_if_required(l,ptr,udIndex,weakIndex);//it is required

			Add_ptr<T
					,typename OOLUA::Proxy_class<T>::AllBases
					,0
					,typename TYPELIST::At_default< typename OOLUA::Proxy_class<T>::AllBases, 0, TYPE::Null_type >::Result
				> addThisTypesBases;
			addThisTypesBases(l,ptr,udIndex,weakIndex);

			lua_pop(l,1);//ud
			return ud;
		}


		template<typename T>
		inline void remove_proxy_ptr(lua_State* /*const*/ l,T * ptr)
		{
			int weakIndex = push_weak_table(l);

			remove_classes_ud_if_required(l,ptr,weakIndex);//it is required
			Set_nill_for_corrisponding_ud<T
					,typename OOLUA::Proxy_class<T>::Bases
					,0
					,typename TYPELIST::At_default< typename OOLUA::Proxy_class<T>::AllBases, 0, TYPE::Null_type >::Result
				> addThisTypesBases;
			addThisTypesBases(l,ptr,weakIndex);
			lua_pop(l,1);//pop the table
		}


		template<typename Type,typename Bases, int BaseIndex,typename BaseType>
		struct Add_ptr
		{
			void operator()(lua_State * const l,Type* ptr,int udIndex,int weakIndex)
			{
				//add this type if needed
				add_ptr_if_required(l,(BaseType*)ptr,udIndex,weakIndex);
				//add the next in the type list if needed
				Add_ptr<
						Type
						,Bases
						,BaseIndex + 1
						,typename TYPELIST::At_default< Bases, BaseIndex + 1, TYPE::Null_type >::Result
					> addBaseNextPtr;
				addBaseNextPtr(l,ptr,udIndex,weakIndex);
			}
		};

		template<typename Type,typename Bases, int BaseIndex>
		struct Add_ptr<Type,Bases,BaseIndex,TYPE::Null_type>
		{
			void operator()(lua_State * const /*l*/,Type* /*ptr*/,int /*udIndex*/,int /*weakIndex*/)const
			{}
		};

		template<typename Type,typename Bases, int BaseIndex,typename BaseType>
		struct Set_nill_for_corrisponding_ud
		{
			void operator()(lua_State * const l,Type* ptr,int weakIndex)
			{
				remove_classes_ud_if_required(l,(BaseType*)ptr,weakIndex);

				Set_nill_for_corrisponding_ud<
						Type
						,Bases
						,BaseIndex + 1
						,typename TYPELIST::At_default< Bases, BaseIndex + 1, TYPE::Null_type >::Result
					> setBaseUdNext;
				setBaseUdNext(l,ptr,weakIndex);
			}
		};

		template<typename Type,typename Bases, int BaseIndex>
		struct Set_nill_for_corrisponding_ud<Type,Bases,BaseIndex,TYPE::Null_type>
		{
			void operator()(lua_State * const /*l*/,Type* /*ptr*/,int /*weakIndex*/)const
			{}
		};


		template<typename Type,typename Bases, int BaseIndex,typename BaseType>
		struct Has_a_root_entry
		{
			void operator()(lua_State * const l,Type* ptr,int weakIndex,bool& result)
			{
				if(result)return;
				result = is_there_an_entry_for_this_void_pointer(l,(BaseType*)ptr,weakIndex);
				if(result)return;
				Has_a_root_entry<
						Type
						,Bases
						,BaseIndex + 1
						,typename TYPELIST::At_default< Bases, BaseIndex + 1, TYPE::Null_type >::Result
					> checkNextBase;
				checkNextBase(l,ptr,weakIndex,result);
			}
		};

		template<typename Type,typename Bases, int BaseIndex>
		struct Has_a_root_entry<Type,Bases,BaseIndex,TYPE::Null_type>
		{
			void operator()(lua_State * const /*l*/,Type* /*ptr*/,int /*weakIndex*/,bool& /*result*/)const
			{}
		};


	}


}

#endif
