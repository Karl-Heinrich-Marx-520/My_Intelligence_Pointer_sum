#define _CRT_SECURE_NO_WARNINGS

#include <atomic>
#include <iostream>
#include <memory>
#include <utility>
#include <cassert>
#include <type_traits>

//-------------------------默认删除器，使用delete操作符销毁对象-----------------------------
template <typename T>
struct default_delete {
	void operator()(T* ptr) const noexcept{
		delete ptr;
	}
};

template <typename T>
struct default_delete<T[]> {
	void operator()(T* ptr) const noexcept{
		delete[] ptr;
	}
};

//------------------------类型擦除删除器核心：抽象基类+模板实现类
//删除器抽象基类：定义删除接口
template <typename T>
struct DeleterBase {
	virtual void destroy(T* ptr) const noexcept = 0; //纯虚函数，定义统一删除接口，支持运行时多态
	virtual ~DeleterBase() noexcept = default; //虚析构函数，确保正确销毁派生类对象
};


//2.模板实现类：封装具体删除器，继承自抽象基类
template <typename T, typename Deleter>
struct ConcreteDeleter : public DeleterBase<T> {
	Deleter deleter; //存储具体删除器对象

	//构造函数，接受具体删除器对象，支持拷贝/移动构造
	explicit ConcreteDeleter(const Deleter& del) noexcept : deleter(std::move(del)) {}
	explicit ConcreteDeleter(Deleter&& del) noexcept : deleter(std::move(del)) {}

	//实现抽象基类的删除接口，调用具体删除器销毁对象
	void destroy(T* ptr) const noexcept override {
		deleter(ptr);
	}
};


//==========================控制块定义=========================
//统一的控制块，包含强引用计数和弱引用计数
template <typename T>
struct ControlBlock {
	std::atomic<size_t> strong_count; // 强引用计数
	std::atomic<size_t> weak_count;   // 弱引用计数
	T* ptr;                           // 指向托管对象的裸指针
	DeleterBase<T>* deleter;          // 删除器

	//构造函数
	template<typename Deleter>
	ControlBlock(T* p, Deleter&& del) noexcept
		:strong_count(1), weak_count(0), ptr(p) 
	{
		// 创建具体删除器实例，擦除类型后存入基类指针
		using DeleterType = std::decay_t<Deleter>; //去除引用/const等修饰
		deleter = new ConcreteDeleter<T, DeleterType>(std::forward<Deleter>(del));
	}

	//析构函数：销毁控制块时，删除器也要销毁
	~ControlBlock() noexcept{
		delete deleter;
	}

	//封装销毁对象的操作，调用删除器销毁托管对象
	void destroy_object() const noexcept {
		if (ptr != nullptr && deleter != nullptr) {
			deleter->destroy(ptr); // 多态调用具体删除器的destroy
			// 销毁后置空指针，避免重复释放
			const_cast<ControlBlock<T>*>(this)->ptr = nullptr;
		}
	}
};

