#pragma once
#include "../define.h"

namespace AHO_NAMESPACE {
	template<typename T>
	struct Index {
		T index;
	};

	template<typename T>
	struct IndexedLine {
		Index<T> idx1, idx2;
	};

	template<typename T>
	struct IndexedTriangle {
		Index<T> idx1, idx2, idx3;
	};
}