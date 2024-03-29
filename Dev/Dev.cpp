﻿template<typename ...parameter>
struct ParameterPack {
//	using Parameters = parameter;
};

template<typename Indispensable, typename T, typename ...Args>
concept initializer = requires(Indispensable i, Args ...args) {
	T(i, args...);
};

#include <iostream>

#include <tuple>

struct A {
	A(void*, int, int) {}
};

template<typename T = A>
struct B {
	template<typename... Args>
	requires initializer<void*, T, Args...>
	B(Args ...args);
};

template<typename T>
template<typename... Args>
requires initializer<void*, T, Args...>
inline B<T>::B(Args ...args) {
	int i = 1;
	T(&i, args...);
}

template<typename T>
consteval bool __sames_as() {
	return true;
}

template<typename T, typename R, typename... Args>
consteval bool __sames_as() {
	if constexpr (std::same_as<T, R>) {
		return __sames_as<T, Args...>();
	}
	return false;
}

template<typename T, typename... Args>
concept sames_as = (0 == sizeof...(Args) && true) ||
				   (1 == sizeof...(Args) && std::same_as<T, Args...>) ||
				   (2 <= sizeof...(Args) && __sames_as<T, Args...>());

int main()
{
	B b(1, 1);
    std::cout << "Hello World!\n";
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
