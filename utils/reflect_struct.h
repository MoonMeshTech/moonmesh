//
// Created by admin on 2025/7/11.
//

#ifndef REFLECT_STRUCT_H
#define REFLECT_STRUCT_H

#include "macro.h"
#include <type_traits>
#include "json.hpp"


template <typename T>
struct reflect_trait {
    template<typename Func>
    static  constexpr  void for_each_members(T & self,Func && func) {
        self.for_each_members(func);
    }\
};

#define REFLECT_TYPE_BEGIN(Type,...)\
template<>\
struct reflect_trait<Type> {\
using  reflect_type = bool;\
template <typename Func>\
static constexpr void for_each_members(Type& stu,Func && func) {\



#define REFLECT_TYPE_PER_MEMBER(name)\
func(#name,stu.name);


#define REFLECT_TYPE_END() \
}\
};


#define REFLECT_TYPE(Type,...)\
REFLECT_TYPE_BEGIN(Type) \
REFLECT_PP_FOREACH(REFLECT_TYPE_PER_MEMBER,__VA_ARGS__)\
REFLECT_TYPE_END()\




#define REFLECT_TYPE_TEMPLATE_BEGIN(Type,...)\
template<__VA_ARGS__>\
struct reflect_trait<REFLECT_CALL(REFLECT_CALL Type)> {\
template <typename Func>\
static constexpr void for_each_members(REFLECT_CALL(REFLECT_CALL Type) & stu,Func && func) {\




#define REFLECT_TYPE_TEMPLATE(Type,...)\
REFLECT_CALL(REFLECT_TYPE_TEMPLATE_BEGIN Type) \
REFLECT_PP_FOREACH(REFLECT_TYPE_PER_MEMBER,__VA_ARGS__)\
REFLECT_TYPE_END()\









#define REFLECT_PER_MEMBER(x) \
    func(#x,x);


#define REFLECT(...)\
    template <typename Func> \
   constexpr void for_each_members(Func && func) {\
        REFLECT_PP_FOREACH(REFLECT_PER_MEMBER,__VA_ARGS__)\
    }\
    using reflect_type = bool;\







template<typename T>
constexpr bool is_reflect_struct_v (...){
    return false;
}


template<typename T>
constexpr bool  is_reflect_struct_v (typename T::reflect_type p){
    return true;
}

template<typename T>
constexpr bool is_reflect_struct_v (typename reflect_trait<T>::reflect_type p){
    return true;
};





template <typename T,std::enable_if_t<!is_reflect_struct_v<T>(false),int> = 0>
nlohmann::json serialize( T &object) {
    nlohmann::json root;
    return root=object;
}

template <typename T>
nlohmann::json serialize( std::vector<T> &vec) {
    nlohmann::json root = nlohmann::json::array();
    for (auto &item : vec) {
        root.push_back(serialize(item));
    }
    return root;
}


// template <typename F,typename S>
// nlohmann::json serialize(std::pair<F,S> & v){
//     nlohmann::json root;

// }

template <typename K, typename V>
nlohmann::json serialize( std::map<K,V> &map) {
    nlohmann::json root;
    for (auto &pair : map) {
        auto value=serialize(pair.second);
        root[pair.first]=value;
    }
    return root;
}

template <typename T,std::enable_if_t<is_reflect_struct_v<T>(false),int> = 0>
  nlohmann::json serialize(T &object) {
    nlohmann::json root;
    reflect_trait<T>::for_each_members(object,[&](const char * key,auto & value) {
        root[key]=serialize(value);
    });
    return root;
}


template <typename T>
void  deserialize(std::vector<T> & vec,const nlohmann::json & root) {
    for (const auto &item : root) {
        T value;
        deserialize<T>(value, item);
        vec.push_back(value);
    }
}

template <typename T,std::enable_if_t<!is_reflect_struct_v<T>(false),int> = 0>
void deserialize(T & object,const nlohmann::json & root) {
   object = root.get<T>();
}

template <typename T,std::enable_if_t<is_reflect_struct_v<T>(false),int> = 0>
void  deserialize(T & object,const nlohmann::json & root) {
    reflect_trait<T>::for_each_members(object,[&](const char * key,auto & value) {
        deserialize(value,root[key]);
    });
}



template <typename K, typename V>
void deserialize(std::map<K,V> & map,const nlohmann::json & root) {
    for (auto it = root.begin(); it != root.end(); ++it) {
        const auto &key = it.key();
        const auto &value = it.value();
        auto & re=map[key];
        deserialize<V>(re,value);
    }
}




#endif //REFLECT_STRUCT_H
