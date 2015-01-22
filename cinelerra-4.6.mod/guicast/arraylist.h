#ifndef ARRAYLIST_H
#define ARRAYLIST_H
#include <stdio.h>
#include <stdlib.h>

template<class TYPE>
class ArrayList {
	int avail;
	int array;
	void reallocate(int n);
	void del(TYPE &value) { if( array ) delete [] value; else delete value; }
	void del_value(int i) { del(values[i]); }
	static int cmpr(TYPE *a, TYPE *b) {
		if( *a == *b ) return 0;
		return *a > *b ? 1 : -1;
	}
public:
	int total;
	TYPE* values;

	void allocate(int total) { if( total >= avail ) reallocate(total); }
	TYPE &append() {
		int i = total++;
		if( total >= avail ) reallocate(total*2);
		return values[i];
	}
	TYPE &append(TYPE value) { return append() = value; }
	TYPE &insert(TYPE value, int n) {
		append();
		for(int i=total; --i>n; ) values[i]=values[i-1];
		return values[n] = value;
	}
	void remove() { --total; }
	void remove_object() {
		if( total > 0 ) { del_value(total-1); remove(); }
		else fprintf(stderr, "ArrayList<TYPE>::remove_object: array is 0 length.\n");
	}
	void remove_number(int n) {
		if( n >= total ) return;
		for(int i=n+1; i<total; ++i ) values[i-1]=values[i];
		remove();
	}
	void remove(TYPE value) {
		int out = 0;
		for( int in=0; in<total; ++in )
			if( values[in] != value ) values[out++] = values[in];
		total = out;
	}
	void remove_object(TYPE value) { remove(value);  del(value); }
	void remove_object_number(int i) {
		if( i < total ) { del_value(i);  remove_number(i); }   
		else fprintf(stderr, "ArrayList<TYPE>::remove_object_number:"
			 " number %d out of range %d.\n", i, total);
	}
	int number_of(TYPE value) {
		for( int i=0; i<total; ++i )
			if( values[i] == value ) return i;
		return -1;
	}
	void remove_all() { total = 0; }
	void remove_all_objects() {
		for( int i=0; i<total; ++i ) del(values[i]);
		total = 0;
	}
	TYPE &last() { return values[total - 1]; }
	void set_array_delete() { array = 1; }
	int size() { return total; }
	TYPE get(int i) {
		if( i < total ) return values[i];
		fprintf(stderr,"ArrayList<TYPE>::get number=%d total=%d\n",i,total);
		return 0;
	}
	TYPE set(int i, TYPE value) {
		if( i < total ) return values[i] = value;
		fprintf(stderr,"ArrayList<TYPE>::set number=%d total=%d\n",i,total);
		return 0;
	}
	TYPE &operator [](int i) { return values[i]; }
	void sort(int (*cmp)(TYPE *a, TYPE *b) = 0) {
		return qsort(values, size(), sizeof(TYPE),
			(int(*)(const void *, const void *))(cmp ? cmp : cmpr));
	}

	ArrayList() { total = array = 0; values = new TYPE[avail = 16]; }
	~ArrayList() { delete [] values; }
};

template<class TYPE>
void ArrayList<TYPE>::reallocate(int n)
{
	TYPE* newvalues = new TYPE[avail=n];
	for( int i=total; --i>=0; ) newvalues[i] = values[i];
	delete [] values;  values = newvalues;
}


#endif
