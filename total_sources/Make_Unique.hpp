#define _CRT_SECURE_NO_WARNINGS

#include "UniquePtr.hpp"

//------------------------------- Make_Unique 实现 -------------------------------
//1.普通类型版本（非数组）：完美转发构造参数
template <typename T, typename... Args>
typename std::enable_if<!std::is_array<T>::value, UniquePtr<T>>::type
Make_Unique(Args&&... args) {
	return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

//2.数组类型版本：接受一个size参数，创建动态数组
template <typename T>
typename std::enable_if<std::is_array<T>::value && std::extent<T>::value == 0,
	UniquePtr<T>>::type
Make_Unique(std::size_t size) {
	using element_type = tpenaem std::remove_extent<T>::type; // 获取数组元素类型
	return UniquePtr<T>(new element_type[size]());
}

//3.禁止对已知大小的数组使用 Make_Unique
template <typename T, typename... Args>
typename std::enable_if<std::is_array<T>::value && std::extent<T>::value != 0,
	UniquePtr<T>>::type
Make_Unique(Args&&...) = delete; // 删除函数，禁止使用


