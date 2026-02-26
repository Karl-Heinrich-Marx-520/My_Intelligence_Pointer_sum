#include "WeakPtr.hpp"

struct Node {
    std::string name;
	WeakPtr<Node> peer; // 使用 WeakPtr 解决循环引用

    Node(const std::string& n) : name(n) {
        std::cout << "Node " << name << " 构造" << std::endl;
	}
    ~Node() {
        std::cout << "Node " << name << " 析构" << std::endl;
	}
};

int main() {
    //测试1：WeakPtr 基础功能测试（构造/expired/use_count/lock）
    {
		// 从 SharedPtr 构造 WeakPtr
        SharedPtr<int> sp(new int(42));
        WeakPtr<int> wp(sp);
        assert(!wp.expired());
		// use_count 应该反映 SharedPtr 的强引用计数
        assert(wp.use_count() == 1);
		// lock 应该成功返回有效的 SharedPtr
        SharedPtr<int> sp2 = wp.lock();
        assert(sp2);
        assert(*sp2 == 42);
		std::cout << "基础功能测试通过！" << std::endl;
	}

	//测试 2：强引用释放后 WeakPtr 过期
    {
        WeakPtr<std::string> wp;

        {
			SharedPtr<std::string> sp(new std::string("Hello"));
            wp = sp;
            std::cout << "1. 局部作用域内：" << std::endl;
            std::cout << "   WeakPtr 是否过期: " << std::boolalpha << wp.expired() << std::endl; // false
            std::cout << "   强计数: " << sp.use_count() << std::endl; // 1
        } // 局部作用域结束，SharedPtr 析构，托管对象销毁

		std::cout << "2. 局部作用域外：" << std::endl;
        std::cout << "   WeakPtr 是否过期: " << std::boolalpha << wp.expired() << std::endl; // true
        std::cout << "   WeakPtr 观测的强计数: " << wp.use_count() << std::endl; // 0

        // 过期后 lock() 返回空 SharedPtr
        SharedPtr<std::string> sp2 = wp.lock();
        if (!sp2) {
            std::cout << "\n3. lock() 结果：返回空 SharedPtr" << std::endl;
        }
    }

    //测试 3：多 WeakPtr 弱引用计数管理
    {
        // 创建带自定义删除器的 SharedPtr
        SharedPtr<int> sp(new int(456));

        // 3 个 WeakPtr 关联同一 SharedPtr
        WeakPtr<int> wp1(sp);
        WeakPtr<int> wp2(sp);
        WeakPtr<int> wp3(sp);

        std::cout << "1. 3 个 WeakPtr 关联后：" << std::endl;
        std::cout << "   SharedPtr 强计数: " << sp.use_count() << std::endl; // 1
        std::cout << "   WeakPtr1 是否过期: " << std::boolalpha << wp1.expired() << std::endl; // false

        // 逐个释放 WeakPtr
        wp1.reset();
        std::cout << "\n2. 释放 wp1 后：" << std::endl;
        std::cout << "   wp2 是否有效: " << !wp2.expired() << std::endl; // true

        wp2.reset();
        wp3.reset();
        std::cout << "\n3. 释放所有 WeakPtr 后：" << std::endl;
        std::cout << "   wp3 是否过期: " << wp3.expired() << std::endl; // true

        // 释放最后一个强引用，触发对象删除 + 控制块销毁
        sp.reset();
    }

    //测试 4：循环引用解决（WeakPtr 核心应用）
    {
        {
            // 创建两个节点，互相持有对方的 WeakPtr
		    SharedPtr<Node> nodeA(new Node("A"));
		    SharedPtr<Node> nodeB(new Node("B"));

		    //用WeakPtr 建立循环引用关系
		    nodeA->peer = nodeB;
		    nodeB->peer = nodeA;

		    // 验证 lock() 能正确获取对方节点
            SharedPtr<Node> peer_of_A = nodeA->peer.lock();
		    std::cout << "\nnodeA 的关联节点：" << peer_of_A->name << std::endl; // 应该输出 "B"
        }
		std::cout << "\n循环引用测试结束，节点应已正确析构！" << std::endl;
    }

	//测试5：赋值和交换
    {
		SharedPtr<int> sp1(new int(100));
		SharedPtr<int> sp2(new int(200));

		//1.拷贝赋值
		WeakPtr<int> wp1(sp1);
        WeakPtr<int> wp2 = wp1;
		std::cout << "1. 拷贝赋值后：" << std::endl;
		std::cout << "wp2 lock() 取值：" << *(wp2.lock()) << std::endl; // 100

		//2.移动赋值
		WeakPtr<int>  wp3(std::move(wp2));
		std::cout << "\n2. 移动赋值后：" << std::endl;
		std::cout << "wp2 是否过期: " << std::boolalpha << wp2.expired() << std::endl; // true
		std::cout << "wp3 lock() 取值：" << *(wp3.lock()) << std::endl; // 100

		//3. 从 SharedPtr 赋值
        wp3 = sp2;
		std::cout << "\n3. 从 SharedPtr 赋值后：" << std::endl;
		std::cout << "wp3 lock() 取值：" << *(wp3.lock()) << std::endl; // 200

        //4. swap交换
		WeakPtr<int> wp4(sp1);
		WeakPtr<int> wp5(sp2);
		std::cout << "\m 4. swap交换前：" << std::endl;
		std::cout << "wp4 lock() 取值：" << *(wp4.lock()) << std::endl; // 100
		std::cout << "wp5 lock() 取值：" << *(wp5.lock()) << std::endl; // 200
		wp4.swap(wp5);
		std::cout << "\n4. swap交换后：" << std::endl;
		std::cout << "wp4 lock() 取值：" << *(wp4.lock()) << std::endl; // 200
		std::cout << "wp5 lock() 取值：" << *(wp5.lock()) << std::endl; // 100
    }

    //测试6：空 WeakPtr 与边界情况
    {
		// 1. 默认构造的 WeakPtr 应该过期
		WeakPtr<int> wp_empty;
		std::cout << "1. 空 WeakPtr 是否过期: " << std::boolalpha << wp_empty.expired() << std::endl; // true
		std::cout << "2. 空 WeakPtr lock() 是否返回空 SharedPtr: " << std::boolalpha << !wp_empty.lock() << std::endl; // true
        
		// 2.从空 SharedPtr 构造 WeakPtr
		SharedPtr<int> sp_empty;
		WeakPtr<int> wp_from_empty(sp_empty);
		std::cout << "\n3. 从空 SharedPtr 构造的 WeakPtr 是否过期: " << std::boolalpha << wp_from_empty.expired() << std::endl; // true
		std::cout << "强计数：" << wp_from_empty.use_count() << std::endl; // 0

		//3.空WeakPtr 的 reset 和 swap 操作
        WeakPtr<int> wp1;
        WeakPtr<int> wp2;
		wp1.swap(wp2); // 交换两个空 WeakPtr 应该安全无效
		wp1.reset(); // 重置空 WeakPtr 应该安全无效
		std::cout << "\n4. 空 WeakPtr 的 reset 和 swap 操作完成，无异常！" << std::endl;
    }

	std::cout << "\n所有测试完成！" << std::endl;
    return 0;
}
