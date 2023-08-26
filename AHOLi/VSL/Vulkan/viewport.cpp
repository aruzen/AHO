#include "viewport.h"

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Viewport<T>::Viewport()
{
}

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Viewport<T>::Viewport(T* t) : __VSLD2Rectangle<T>(t) {}

template<VSL_NAMESPACE::is_rectangle T>
VSL_NAMESPACE::Viewport<T>::Viewport(const element_type& x, const element_type& y, const element_type& width, const element_type& height)
{
	this->x = x;
	this->y = y;
	this->width = width;
	this->height = height
}
