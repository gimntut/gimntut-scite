// Scintilla source code edit control
/** @file LexCrontab.cxx
 ** Lexer to use with extended crontab files used by a powerful
 ** Windows scheduler/event monitor/automation manager nnCron.
 ** (http://nemtsev.eserv.ru/)
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "Platform.h"

#include "PropSet.h"
#include "Accessor.h"
#include "KeyWords.h"
#include "Scintilla.h"
#include "SciLexer.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

bool is_whitespace(int ch){
    return ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ';
}

bool is_blank(int ch){
    return ch == '\t' || ch == ' ';
}
//#define FORTH_DEBUG
#ifdef FORTH_DEBUG
static FILE *f_debug;
#define log(x)  fputs(f_debug,x);
#else
#define log(x)
#endif

#define STATE_LOCALE
#define BL ' '
#define DEFWORD_FLAG 0x40 //!-add-[ForthImprovement]
#define FORTH_STYLE_MASK 0x1f //!-add-[ForthImprovement]

static Accessor *st;
static int cur_pos,pos1,pos2,pos0,lengthDoc;
char *buffer;

char getChar(bool is_bl){
    char ch=st->SafeGetCharAt(cur_pos);
    if(is_bl) if(is_whitespace(ch)) ch=BL;
    return ch;
}

char getCharBL(){
    char ch=st->SafeGetCharAt(cur_pos);
    return ch;
}
bool is_eol(char ch){
    return ch=='\n' || ch=='\r';
}

int parse(char ch, bool skip_eol){
// pos1 - start pos of word
// pos2 - pos after of word
// pos0 - start pos
    char c=0;
    int len;
    bool is_bl=ch==BL;
    pos0=pos1=pos2=cur_pos;
    for(;cur_pos<lengthDoc && (c=getChar(is_bl))==ch; cur_pos++){
        if(is_eol(c) && !skip_eol){
            pos2=pos1;
            return 0;
        }
    }
    pos1=cur_pos;
    pos2=pos1;
    if(cur_pos==lengthDoc) return 0;
    for(len=0;cur_pos<lengthDoc && (c=getChar(is_bl))!=ch; cur_pos++){
        if(is_eol(c) && !skip_eol) break;
        pos2++;
        buffer[len++]=c;
    }
    if(c==ch) pos2--;
    buffer[len]='\0';
#ifdef FORTH_DEBUG
    fprintf(f_debug,"parse: %c %s\n",ch,buffer);
#endif
    return len;
}

bool _is_number(char *s,int base){
    for(;*s;s++){
        int digit=((int)*s)-(int)'0';
#ifdef FORTH_DEBUG
    fprintf(f_debug,"digit: %c %d\n",*s,digit);
#endif
        if(digit>9 && base>10) digit-=7;
        if(digit<0) return false;
        if(digit>=base) return false;
    }
    return true;
}

bool is_number(char *s){
    if(strncmp(s,"0x",2)==0) return _is_number(s+2,16);
    return _is_number(s,10);
}

//!-start-[ForthImprovement]
static void RollbackToDefStart(unsigned int startPos){
    // rollback to start of definition
    cur_pos = startPos;
    while(cur_pos>0 && st->StyleAt(cur_pos)!=SCE_FORTH_DEFAULT){
        cur_pos--;
    }
    cur_pos = st->LineStart(st->GetLine(cur_pos));
    st->Flush();
    st->StartAt(cur_pos,static_cast<char>(STYLE_MAX));
    st->StartSegment(cur_pos);
}
//!-end-[ForthImprovement]

//!static void ColouriseForthDoc(unsigned int startPos, int length, int, WordList *keywordLists[], Accessor &styler)
static void ColouriseForthDoc(unsigned int startPos, int length, int initStyle, WordList *keywordLists[], Accessor &styler) //!-change-[ForthImprovement]
{
    st=&styler;
    cur_pos=startPos;
    lengthDoc = startPos + length;
    buffer = new char[length];

#ifdef FORTH_DEBUG
    f_debug=fopen("c:\\sci.log","at");
#endif

    WordList &control = *keywordLists[0];
    WordList &keyword = *keywordLists[1];
    WordList &defword = *keywordLists[2];
    WordList &preword1 = *keywordLists[3];
    WordList &preword2 = *keywordLists[4];
    WordList &strings = *keywordLists[5];
//!-start-[ForthImprovement]
    WordList &startdefword = *keywordLists[6];
    WordList &enddefword = *keywordLists[7];
    WordList &gui = *keywordLists[10];
    WordList &oop = *keywordLists[11];
    WordList &word1 = *keywordLists[12];
    WordList &word2 = *keywordLists[13];
    WordList &word3 = *keywordLists[14];
    WordList &word4 = *keywordLists[15];
    
    bool isInDefinition = (initStyle&DEFWORD_FLAG) == DEFWORD_FLAG; // flag for inside definition tags state
    bool isPossibleRollback = isInDefinition; // flag for possible undefined state by start pos
#ifdef FORTH_DEBUG
    fprintf(f_debug,"\nColouriseForthDoc: %d %d %d %d\n",startPos,length,initStyle,isInDefinition);
#endif
//!-end-[ForthImprovement]

    // go through all provided text segment
    // using the hand-written state machine shown below
//!    styler.StartAt(startPos);
    styler.StartAt(startPos,static_cast<char>(STYLE_MAX)); //!-change-[ForthImprovement]
    styler.StartSegment(startPos);
//!    while(parse(BL,true)!=0){
//!-start-[ForthImprovement]
    if((initStyle&FORTH_STYLE_MASK) == SCE_FORTH_STRING){
        // while in tags [ ]
        while(parse(BL,true)!=0){
            if(isInDefinition && isPossibleRollback && enddefword.InList(buffer)){
                // rollback to start of definition because find enddefword before ]
                RollbackToDefStart(startPos);
                isInDefinition = false;
                isPossibleRollback = false;
                break;
            }else
            if(strcmp("]",buffer)==0){
                styler.ColourTo(cur_pos,SCE_FORTH_STRING|(isInDefinition?DEFWORD_FLAG:0));
                break;
            }
        }
    }
    while(parse(BL,true)!=0)
    if(isInDefinition){
        if(pos0!=pos1){
            styler.ColourTo(pos0,SCE_FORTH_DEFAULT|DEFWORD_FLAG);
            styler.ColourTo(pos1-1,SCE_FORTH_DEFAULT|DEFWORD_FLAG);
        }
        if(strcmp("\\",buffer)==0){
            styler.ColourTo(pos1,SCE_FORTH_COMMENT|DEFWORD_FLAG);
            parse(1,false);
            styler.ColourTo(pos2,SCE_FORTH_COMMENT|DEFWORD_FLAG);
        }else if(strcmp("(",buffer)==0){
            styler.ColourTo(pos1,SCE_FORTH_COMMENT|DEFWORD_FLAG);
            parse(')',true);
            if(cur_pos<lengthDoc) cur_pos++;
            styler.ColourTo(cur_pos,SCE_FORTH_COMMENT|DEFWORD_FLAG);
        }else if(strcmp("[",buffer)==0){
            isPossibleRollback = false;
            int p1 = pos1;
            bool isString = false;
            while(parse(BL,true)!=0){
                if(enddefword.InList(buffer)){
                    break;
                }else if(strcmp("]",buffer)==0){
                    isString = true;
                    break;
                }
            }
            if(isString){
                styler.ColourTo(p1,SCE_FORTH_STRING|DEFWORD_FLAG);
                styler.ColourTo(cur_pos,SCE_FORTH_STRING|DEFWORD_FLAG);
            }else{
                cur_pos = p1+1;
                styler.ColourTo(cur_pos,SCE_FORTH_DEFAULT|DEFWORD_FLAG);
            }
        }else if(isPossibleRollback && strcmp("]",buffer)==0){
            // rollback to start of definition because find ] before [
            RollbackToDefStart(startPos);
            isInDefinition = false;
            isPossibleRollback = false;
        }else if(strcmp("{",buffer)==0){
            styler.ColourTo(pos1,SCE_FORTH_LOCALE|DEFWORD_FLAG);
            parse('}',false);
            if(cur_pos<lengthDoc) cur_pos++;
            styler.ColourTo(cur_pos,SCE_FORTH_LOCALE|DEFWORD_FLAG);
        }else if(strings.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_STRING|DEFWORD_FLAG);
            parse('"',false);
            if(cur_pos<lengthDoc) cur_pos++;
            styler.ColourTo(cur_pos,SCE_FORTH_STRING|DEFWORD_FLAG);
        }else if(enddefword.InList(buffer)) {
            isInDefinition = false;
            isPossibleRollback = false;
            styler.ColourTo(pos2,SCE_FORTH_KEYWORD);
        }else if(control.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_CONTROL|DEFWORD_FLAG);
            styler.ColourTo(pos2,SCE_FORTH_CONTROL|DEFWORD_FLAG);
        }else if(keyword.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_KEYWORD|DEFWORD_FLAG);
            styler.ColourTo(pos2,SCE_FORTH_KEYWORD|DEFWORD_FLAG);
        }else if(preword1.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_PREWORD1|DEFWORD_FLAG);
            parse(BL,false);
            styler.ColourTo(pos2,SCE_FORTH_PREWORD1|DEFWORD_FLAG);
        }else if(preword2.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_PREWORD2|DEFWORD_FLAG);
            parse(BL,false);
            styler.ColourTo(pos2,SCE_FORTH_PREWORD2|DEFWORD_FLAG);
            parse(BL,false);
            styler.ColourTo(pos1,SCE_FORTH_STRING|DEFWORD_FLAG);
            styler.ColourTo(pos2,SCE_FORTH_STRING|DEFWORD_FLAG);
        }else if(gui.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_GUI|DEFWORD_FLAG);
        }else if(oop.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_OOP|DEFWORD_FLAG);
        }else if(word1.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_WORD1|DEFWORD_FLAG);
        }else if(word2.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_WORD2|DEFWORD_FLAG);
        }else if(word3.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_WORD3|DEFWORD_FLAG);
        }else if(word4.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_WORD4|DEFWORD_FLAG);
        }else if(is_number(buffer)){
            styler.ColourTo(pos1,SCE_FORTH_NUMBER|DEFWORD_FLAG);
            styler.ColourTo(pos2,SCE_FORTH_NUMBER|DEFWORD_FLAG);
        }
    }else{
//!-end-[ForthImprovement]
        if(pos0!=pos1){
            styler.ColourTo(pos0,SCE_FORTH_DEFAULT);
            styler.ColourTo(pos1-1,SCE_FORTH_DEFAULT);
        }
        if(strcmp("\\",buffer)==0){
            styler.ColourTo(pos1,SCE_FORTH_COMMENT);
            parse(1,false);
            styler.ColourTo(pos2,SCE_FORTH_COMMENT);
        }else if(strcmp("(",buffer)==0){
            styler.ColourTo(pos1,SCE_FORTH_COMMENT);
            parse(')',true);
            if(cur_pos<lengthDoc) cur_pos++;
            styler.ColourTo(cur_pos,SCE_FORTH_COMMENT);
        }else if(strcmp("[",buffer)==0){
            styler.ColourTo(pos1,SCE_FORTH_STRING);
//!-start-[ForthImprovement]
//!            parse(']',true);
//!            if(cur_pos<lengthDoc) cur_pos++;
            while(parse(BL,true)!=0)
                if(strcmp("]",buffer)==0)
                    break;
//!-end-[ForthImprovement]
            styler.ColourTo(cur_pos,SCE_FORTH_STRING);
        }else if(strcmp("{",buffer)==0){
            styler.ColourTo(pos1,SCE_FORTH_LOCALE);
            parse('}',false);
            if(cur_pos<lengthDoc) cur_pos++;
            styler.ColourTo(cur_pos,SCE_FORTH_LOCALE);
        }else if(strings.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_STRING);
            parse('"',false);
            if(cur_pos<lengthDoc) cur_pos++;
            styler.ColourTo(cur_pos,SCE_FORTH_STRING);
//!-start-[ForthImprovement]
        }else if(startdefword.InList(buffer)) {
            isInDefinition = true;
            styler.ColourTo(pos1,SCE_FORTH_KEYWORD);
            styler.ColourTo(pos2,SCE_FORTH_KEYWORD);
            if(defword.InList(buffer)) {
                parse(BL,false);
                styler.ColourTo(pos1-1,SCE_FORTH_DEFAULT|DEFWORD_FLAG);
                styler.ColourTo(pos1,SCE_FORTH_DEFWORD|DEFWORD_FLAG);
                styler.ColourTo(pos2,SCE_FORTH_DEFWORD|DEFWORD_FLAG);
            }
//!-end-[ForthImprovement]
        }else if(control.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_CONTROL);
            styler.ColourTo(pos2,SCE_FORTH_CONTROL);
        }else if(keyword.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_KEYWORD);
            styler.ColourTo(pos2,SCE_FORTH_KEYWORD);
        }else if(defword.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_KEYWORD);
            styler.ColourTo(pos2,SCE_FORTH_KEYWORD);
            parse(BL,false);
            styler.ColourTo(pos1-1,SCE_FORTH_DEFAULT);
            styler.ColourTo(pos1,SCE_FORTH_DEFWORD);
            styler.ColourTo(pos2,SCE_FORTH_DEFWORD);
        }else if(preword1.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_PREWORD1);
            parse(BL,false);
            styler.ColourTo(pos2,SCE_FORTH_PREWORD1);
        }else if(preword2.InList(buffer)) {
            styler.ColourTo(pos1,SCE_FORTH_PREWORD2);
            parse(BL,false);
            styler.ColourTo(pos2,SCE_FORTH_PREWORD2);
            parse(BL,false);
            styler.ColourTo(pos1,SCE_FORTH_STRING);
            styler.ColourTo(pos2,SCE_FORTH_STRING);
//!-start-[ForthImprovement]
        }else if(gui.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_GUI);
        }else if(oop.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_OOP);
        }else if(word1.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_WORD1);
        }else if(word2.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_WORD2);
        }else if(word3.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_WORD3);
        }else if(word4.InList(buffer)) {
            styler.ColourTo(pos2,SCE_FORTH_WORD4);
//!-end-[ForthImprovement]
        }else if(is_number(buffer)){
            styler.ColourTo(pos1,SCE_FORTH_NUMBER);
            styler.ColourTo(pos2,SCE_FORTH_NUMBER);
        }
    }
#ifdef FORTH_DEBUG
    fclose(f_debug);
#endif
    delete []buffer;
    return;
/*
                        if(control.InList(buffer)) {
                            styler.ColourTo(i,SCE_FORTH_CONTROL);
                        } else if(keyword.InList(buffer)) {
                            styler.ColourTo(i-1,SCE_FORTH_KEYWORD );
                        } else if(defword.InList(buffer)) {
                            styler.ColourTo(i-1,SCE_FORTH_DEFWORD );
//                            prev_state=SCE_FORTH_DEFWORD
                        } else if(preword1.InList(buffer)) {
                            styler.ColourTo(i-1,SCE_FORTH_PREWORD1 );
//                            state=SCE_FORTH_PREWORD1;
                        } else if(preword2.InList(buffer)) {
                            styler.ColourTo(i-1,SCE_FORTH_PREWORD2 );
                         } else {
                            styler.ColourTo(i-1,SCE_FORTH_DEFAULT);
                        }
*/
/*
    chPrev=' ';
    for (int i = startPos; i < lengthDoc; i++) {
        char ch = chNext;
        chNext = styler.SafeGetCharAt(i + 1);
        if(i!=startPos) chPrev=styler.SafeGetCharAt(i - 1);

        if (styler.IsLeadByte(ch)) {
            chNext = styler.SafeGetCharAt(i + 2);
            i++;
            continue;
        }
#ifdef FORTH_DEBUG
        fprintf(f_debug,"%c %d ",ch,state);
#endif
        switch(state) {
            case SCE_FORTH_DEFAULT:
                if(is_whitespace(ch)) {
                    // whitespace is simply ignored here...
                    styler.ColourTo(i,SCE_FORTH_DEFAULT);
                    break;
                } else if( ch == '\\' && is_blank(chNext)) {
                    // signals the start of an one line comment...
                    state = SCE_FORTH_COMMENT;
                    styler.ColourTo(i,SCE_FORTH_COMMENT);
                } else if( is_whitespace(chPrev) &&  ch == '(' &&  is_whitespace(chNext)) {
                    // signals the start of a plain comment...
                    state = SCE_FORTH_COMMENT_ML;
                    styler.ColourTo(i,SCE_FORTH_COMMENT_ML);
                } else if( isdigit(ch) ) {
                    // signals the start of a number
                    bufferCount = 0;
                    buffer[bufferCount++] = ch;
                    state = SCE_FORTH_NUMBER;
                } else if( !is_whitespace(ch)) {
                    // signals the start of an identifier
                    bufferCount = 0;
                    buffer[bufferCount++] = ch;
                    state = SCE_FORTH_IDENTIFIER;
                } else {
                    // style it the default style..
                    styler.ColourTo(i,SCE_FORTH_DEFAULT);
                }
                break;

            case SCE_FORTH_COMMENT:
                // if we find a newline here,
                // we simply go to default state
                // else continue to work on it...
                if( ch == '\n' || ch == '\r' ) {
                    state = SCE_FORTH_DEFAULT;
                } else {
                    styler.ColourTo(i,SCE_FORTH_COMMENT);
                }
                break;

            case SCE_FORTH_COMMENT_ML:
                if( ch == ')') {
                    state = SCE_FORTH_DEFAULT;
                } else {
                    styler.ColourTo(i+1,SCE_FORTH_COMMENT_ML);
                }
                break;

            case SCE_FORTH_IDENTIFIER:
                // stay  in CONF_IDENTIFIER state until we find a non-alphanumeric
                if( !is_whitespace(ch) ) {
                    buffer[bufferCount++] = ch;
                } else {
                    state = SCE_FORTH_DEFAULT;
                    buffer[bufferCount] = '\0';
#ifdef FORTH_DEBUG
        fprintf(f_debug,"\nid %s\n",buffer);
#endif

                    // check if the buffer contains a keyword,
                    // and highlight it if it is a keyword...
//                    switch(prev_state)
//                    case SCE_FORTH_DEFAULT:
                        if(control.InList(buffer)) {
                            styler.ColourTo(i,SCE_FORTH_CONTROL);
                        } else if(keyword.InList(buffer)) {
                            styler.ColourTo(i-1,SCE_FORTH_KEYWORD );
                        } else if(defword.InList(buffer)) {
                            styler.ColourTo(i-1,SCE_FORTH_DEFWORD );
//                            prev_state=SCE_FORTH_DEFWORD
                        } else if(preword1.InList(buffer)) {
                            styler.ColourTo(i-1,SCE_FORTH_PREWORD1 );
//                            state=SCE_FORTH_PREWORD1;
                        } else if(preword2.InList(buffer)) {
                            styler.ColourTo(i-1,SCE_FORTH_PREWORD2 );
                         } else {
                            styler.ColourTo(i-1,SCE_FORTH_DEFAULT);
                        }
//                        break;
//                    case

                    // push back the faulty character
                    chNext = styler[i--];
                }
                break;

            case SCE_FORTH_NUMBER:
                // stay  in CONF_NUMBER state until we find a non-numeric
                if( isdigit(ch) ) {
                    buffer[bufferCount++] = ch;
                } else {
                    state = SCE_FORTH_DEFAULT;
                    buffer[bufferCount] = '\0';
                    // Colourize here... (normal number)
                    styler.ColourTo(i-1,SCE_FORTH_NUMBER);
                    // push back a character
                    chNext = styler[i--];
                }
                break;
        }
    }
#ifdef FORTH_DEBUG
    fclose(f_debug);
#endif
    delete []buffer;
*/
}

//!static void FoldForthDoc(unsigned int, int, int, WordList *[],
//!                       Accessor &) {
//!-start-[ForthImprovement]
static void FoldForthDoc(unsigned int startPos, int length, int initStyle,
    WordList *keywordlists[], Accessor &styler)
{
    WordList &startdefword = *keywordlists[6];
    WordList &enddefword = *keywordlists[7];
    WordList &fold_begin = *keywordlists[8];
    WordList &fold_end = *keywordlists[9];

    int line = styler.GetLine(startPos);
    int level = styler.LevelAt(line);
    int levelIndent = 0;
    unsigned int endPos = startPos + length;
    char word[256];
    int wordlen = 0;
    int style = initStyle & FORTH_STYLE_MASK;
    int wordstyle = style;
    // Scan for tokens
    for (unsigned int i = startPos; i < endPos; i++) {
        int c = styler.SafeGetCharAt(i, '\n');
        style = styler.StyleAt(i) & FORTH_STYLE_MASK;
        if (is_whitespace(c)) {
            if (wordlen) { // done with token
                word[wordlen] = '\0';
                // CheckFoldPoint
                if (wordstyle == SCE_FORTH_KEYWORD && startdefword.InList(word)) {
                    levelIndent += 1;
                } else
                if (wordstyle == SCE_FORTH_KEYWORD && enddefword.InList(word)) {
                    levelIndent -= 1;
                } else
                if (fold_begin.InList(word)) {
                    levelIndent += 1;
                } else
                if (fold_end.InList(word)) {
                    levelIndent -= 1;
                }
                wordlen = 0;
            }
        }
        else if (!(style == SCE_FORTH_COMMENT || style == SCE_FORTH_COMMENT_ML
            || style == SCE_FORTH_LOCALE || style == SCE_FORTH_STRING
            || style == SCE_FORTH_DEFWORD || style == SCE_FORTH_PREWORD1
            || style == SCE_FORTH_PREWORD2)) {
            if (wordlen) {
                if (wordlen < 255) {
                    word[wordlen] = c;
                    wordlen++;
                }
            } else { // start scanning at first word character
                word[0] = c;
                wordlen = 1;
                wordstyle = style;
            }
        }
        if (c == '\n') { // line end
            if (levelIndent > 0) {
                level |= SC_FOLDLEVELHEADERFLAG;
            }
            if (level != styler.LevelAt(line))
                styler.SetLevel(line, level);
            level += levelIndent;
            if ((level & SC_FOLDLEVELNUMBERMASK) < SC_FOLDLEVELBASE)
                level = SC_FOLDLEVELBASE;
            line++;
            // reset state
            levelIndent = 0;
            level &= ~SC_FOLDLEVELHEADERFLAG;
            level &= ~SC_FOLDLEVELWHITEFLAG;
        }
    }
//!-end-[ForthImprovement]
}

static const char * const forthWordLists[] = {
            "control keywords",
            "keywords",
            "definition words",
            "prewords with one argument",
            "prewords with two arguments",
            "string definition keywords",
//!-start-[ForthImprovement]
            "folding start words",
            "folding end words",
            "GUI",
            "OOP",
            "user defined words 1",
            "user defined words 2",
            "user defined words 3",
            "user defined words 4",
//!-end-[ForthImprovement]
            0,
        };

//!LexerModule lmForth(SCLEX_FORTH, ColouriseForthDoc, "forth",FoldForthDoc,forthWordLists,7);
LexerModule lmForth(SCLEX_FORTH, ColouriseForthDoc, "forth",FoldForthDoc,forthWordLists, 7); //!-change-[ForthImprovement]
