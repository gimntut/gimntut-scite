-- ���� ���� �������� ��� �������� SciTE
-- ����� �� �������� ��� ��� �������� ����������� ������������ ��������, ��������� ��� ���������� ������ ���������, ����������� �� ��� �������� � ������������ ������ � �������� ������ ��� ������ ���������������� ������ ���� Tools.
-- ����� (� ������� dofile) �������� ������ �������, �������������� ������� ���������.

----[[ C O M M O N ]]-------------------------------------------------------

-- ����������� ����� � ������ ���������, ��������������� �� ������ ��������
dofile (props["SciteDefaultHome"].."\\tools\\COMMON.lua")

-- ��������� ������ � ��������������� ��������
dofile (props["SciteDefaultHome"].."\\tools\\macro_support.lua")

----[[ � � � � � � � � � ]]-------------------------------------------------

-- ����� ��������� Win1251/DOS866 � ��������� ������� ��������� � ������ ���������
dofile (props["SciteDefaultHome"].."\\tools\\CodePage.lua")

----[[ � � � � � � � � �   � � � � � � ]]-----------------------------------

-- ����� ����� �������� ������� � ��������� ������
dofile (props["SciteDefaultHome"].."\\tools\\lexer_name.lua")

----[[ � � � � � � ]]-------------------------------------------------------

-- ����� ������� ������� (Ctrl+F11)
dofile (props["SciteDefaultHome"].."\\tools\\FontChanger.lua")

-- ��� ��������� �������� ������� ������ (Ctrl+-), �������������� � ��������� �� ������� ����� � ���������� � ������ ���������
dofile (props["SciteDefaultHome"].."\\tools\\Zoom.lua")

----[[ � � � � � � � � � � ]]--------------------------------------------------

-- ���������� ���������� ��������� SciTE, ���������� ����� ����
dofile (props["SciteDefaultHome"].."\\tools\\save_settings.lua")

-- �������������� �������� ��������� ����� ������������� ������
dofile (props["SciteDefaultHome"].."\\tools\\auto_backup.lua")

-- ����� ����������������� ������� ���������� ������� ������� ��� �������� SciTE
-- (���� � SciTEGlobal.properties ����������� ��������� session.manager=1 � save.session.manager.on.quit=1)
dofile (props["SciteDefaultHome"].."\\tools\\SessionManager\\SessionManager.lua")

----[[ R E A D   O N L Y ]]-------------------------------------------------

-- ������ ����������� ������� "Read-Only"
-- ������ ��� ������� �� ��������� ��� �������������� � ���������� �������� � ��������� ������
dofile (props["SciteDefaultHome"].."\\tools\\ReadOnly.lua")

-- ��� �������� ReadOnly, Hidden, System ������ �������� ����� ReadOnly � SciTE
dofile (props["SciteDefaultHome"].."\\tools\\ROCheck.lua")

-- ��������� ���������� RO ������
dofile (props["SciteDefaultHome"].."\\tools\\ROWrite.lua")

----[[ � � � � � �   � � � � � � � � � � � ]]-------------------------------

-- ������������ ������
dofile (props["SciteDefaultHome"].."\\tools\\smartbraces.lua")

-- ������������ HTML �����
dofile (props["SciteDefaultHome"].."\\tools\\html_tags_autoclose.lua")

-- ������������� ��������������� � ������ ������������ (�� Ctrl+Q)
dofile (props["SciteDefaultHome"].."\\tools\\xComment.lua")
--~ dofile (props["SciteDefaultHome"].."\\tools\\smartcomment.lua")

----[[ � � � � � � �  � � � � ]]----------------------------------------------

-- ������ ����������� ������� SciTE "������� ���������� ����"
dofile (props["SciteDefaultHome"].."\\tools\\Open_Selected_Filename.lua")

-- ���������� ����������� ������� SciTE "������� ���������� ����" (��������� ��� ���������������� ���������)
-- � ����� ����������� ������� ���� �� �������� ����� ���� �� ��� ����� ��� ������� ������� Ctrl.
dofile (props["SciteDefaultHome"].."\\tools\\Select_And_Open_Filename.lua")

----[[ � � � � � � � � � � � � � ]]-------------------------------------------

-- ��� �������� �� �������� ������, ������������ �����, �������� ������� ������� �� ������
dofile (props["SciteDefaultHome"].."\\tools\\goto_line.lua")

-- �������� ����������� ������� SciTE "File|New" (Ctrl+N). ������� ����� ����� � ������� �������� � ����������� �������� �����
dofile (props["SciteDefaultHome"].."\\tools\\new_file.lua")

-- �������� HTML ��������� ��� ������ ��� ����������, ����������� �� ���� "�������� HTML-����" Internet Explorer
dofile (props["SciteDefaultHome"].."\\tools\\set_html.lua")

-- �������������� ������� �������, ��������� � �������� ��� ��������� �������� ������ �����.
dofile (props["SciteDefaultHome"].."\\tools\\RestoreRecent.lua")

-- ������� ��� ��������� ������
dofile (props["SciteDefaultHome"].."\\tools\\FoldText.lua")

-- �������������� �������� �� �������� � ����������
dofile (props["SciteDefaultHome"].."\\tools\\AutocompleteObject.lua")

-- ����� ������ ����������� (�� Ctrl+B ��� �������������) ��� �������� ������������ ������������
dofile (props["SciteDefaultHome"].."\\tools\\abbrevlist.lua")

-- ������� ����������� ��������� (Ctrl+Shift+Space) �� �������� �����
dofile (props["SciteDefaultHome"].."\\tools\\ShowCalltip.lua")

-- ���� ��������� ������, ������� ��������� � ������� ������ ��� ����������
dofile (props["SciteDefaultHome"].."\\tools\\highlighting_identical_text.lua")

-- ���������, �����������, �������, �������� ������ ����� � HTML
dofile (props["SciteDefaultHome"].."\\tools\\paired_tags.lua")

-- ��������� ������ � ������ � �������� �� � �������� ��� ����� � ������� Ctrl
dofile (props["SciteDefaultHome"].."\\tools\\HighlightLinks.lua")

-- ����������� ���������� ������ ����������� ��� ini, inf, reg ������
dofile (props["SciteDefaultHome"].."\\tools\\ChangeCommentChar.lua")

----[[ � � � � � � � � � � � � � �  � � � � ]]--------------------------------

-- ����� ���������� ������� "����� � ������..." ������� ����� � ����������� ���� ������� - "������� ��������� �����"
dofile (props["SciteDefaultHome"].."\\tools\\OpenFindFiles.lua")

-- ������� � ����������� ���� ���� (�������) ������� ��� ������ SVN
dofile (props["SciteDefaultHome"].."\\tools\\svn_menu.lua")

----[[ � � � � � � �  �  � � � � � � � � � � � ]]-----------------------------

-- SideBar: ������������������� ������� ������
dofile (props["SciteDefaultHome"].."\\tools\\SideBar.lua")

-- Color Image Viewer: ������������ ����� � �����������
dofile (props["SciteDefaultHome"].."\\tools\\CIViewer\\CIViewer.lua")

-- SciTE_HexEdit: A Self-Contained Primitive Hex Editor for SciTE
dofile (props["SciteDefaultHome"].."\\tools\\HexEdit\\SciTEHexEdit.lua")

-- SciTE Calculator
dofile (props["SciteDefaultHome"].."\\tools\\Calculator\\SciTECalculatorPD.lua")

-- ������� ������������ (�,�,�,�,�) �� ��������������� ������ (��� HTML ����������� �� �����������)
dofile (props["SciteDefaultHome"].."\\tools\\InsertSpecialChar.lua")

----[[ � � � � � � � � �   � � � � � � � � � � ]]-----------------------------

-- ��������� ������� ������� ��������� � ���� �������
local tab_width = tonumber(props['output.tabsize'])
if tab_width ~= nil then
	scite.SendOutput(SCI_SETTABWIDTH, tab_width)
end

----[[ � � � � � � �  � � � � � � � ]]-----------------------------

-- �������� ������ ��� ����������� ����� ���������������� zog
dofile (props["SciteDefaultHome"].."\\languages\\zog.lua")

------------------------------------------------------------------------------
