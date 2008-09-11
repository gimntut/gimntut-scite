#ifndef __TWL_LISTVIEW_H
#define __TWL_LISTVIEW_H
#include "twl_imagelist.h"
class EXPORT TListViewB: public TNotifyWin {
	int m_last_col,m_last_row;
	bool m_has_images, m_custom_paint;
	unsigned int m_fg, m_bg;
	
public:
	TListViewB(TWin* form, bool large_icons = false, bool multiple_columns = false, bool single_select = true);
	void set_image_list(TImageList* il_small, TImageList* il_large = NULL);
	void add_column(const char* label, int width);
	void autosize_column(int col, bool by_contents);
	void start_items();
	int add_item_at(int i, const char* text, int idx = 0, void* data = NULL);
	int add_item(const char* text, int idx = 0, void* data = NULL);
	void add_subitem(int i, const char* text, int sub_idx);
	void delete_item(int i);
	void get_item_text(int i, char* buff, int buffsize);
	void* get_item_data(int i);
    int  selected_id();
    int  count();
	int  columns();
    void clear();
	void set_foreground(unsigned int colour);
	void set_background(unsigned int colour);

	virtual void handle_select(int id) = 0;
	virtual void handle_double_click(int id) = 0;

	// override
    int handle_notify(void *p);
};

class EXPORT TListView: public TListViewB {
	TEventWindow* m_form;
	SelectionHandler m_on_select, m_on_double_click;
public:
	TListView(TEventWindow* form, bool large_icons = false, bool multiple_columns = false, bool single_select = true);
	void on_select(SelectionHandler handler)
	{ m_on_select = handler; }
	void on_double_click(SelectionHandler handler)
	{ m_on_double_click = handler; }

	// implement
	virtual void handle_select(int id);
	virtual void handle_double_click(int id);

};
#endif