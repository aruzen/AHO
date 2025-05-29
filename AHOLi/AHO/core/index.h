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

        Index<T>& operator[] (size_t t);
	};

    template<typename T>
	struct IndexedTriangle {
		Index<T> idx1, idx2, idx3;

        Index<T>& operator[] (size_t t);
	};

    template<typename T>
    Index<T>& IndexedLine<T>::operator[](size_t t) {
        switch (t % 2) {
            case 0:
                return idx1;
            case 1:
                return idx2;
        }
    }

    template<typename T>
    Index<T>& IndexedTriangle<T>::operator[](size_t t) {
        switch (t % 3) {
            case 0:
                return idx1;
            case 1:
                return idx2;
            case 2:
                return idx3;
            default:
                return idx1;
        }
    }
}