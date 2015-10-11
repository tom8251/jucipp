#ifndef JUCI_SOURCE_H_
#define JUCI_SOURCE_H_
#include <iostream>
#include <unordered_map>
#include <vector>
#include "gtkmm.h"
#include "clangmm.h"
#include <thread>
#include <mutex>
#include <string>
#include <atomic>
#include "gtksourceviewmm.h"
#include "terminal.h"
#include "tooltips.h"
#include "selectiondialog.h"
#include <set>
#include <regex>
#include <aspell.h>
#include <boost/property_tree/xml_parser.hpp>

namespace Source {
  Glib::RefPtr<Gsv::Language> guess_language(const boost::filesystem::path &file_path);
  
  class Config {
  public:
    class DocumentationSearch {
    public:
      std::string separator;
      std::unordered_map<std::string, std::string> queries;
    };
    
    std::string style;
    std::string font;
    std::string spellcheck_language;
    
    bool show_map;
    std::string map_font_size;
    
    bool auto_tab_char_and_size;
    char default_tab_char;
    unsigned default_tab_size;
    bool wrap_lines;
    bool highlight_current_line;
    bool show_line_numbers;
    std::unordered_map<std::string, std::string> clang_types;
    
    std::unordered_map<std::string, DocumentationSearch> documentation_searches;
  };
  
  class Token {
  public:
    Token(): type(-1) {}
    Token(int type, const std::string &spelling, const std::string &usr): 
      type(type), spelling(spelling), usr(usr) {}
    operator bool() const {return (type>=0 && spelling.size()>0 && usr.size()>0);}
    bool operator==(const Token &o) const {return (type==o.type &&
                                                   spelling==o.spelling &&
                                                   usr==o.usr);}
    bool operator!=(const Token &o) const {return !(*this==o);}
    
    int type;
    std::string spelling;
    std::string usr;
  };
  
  class FixIt {
  public:
    class Offset {
    public:
      Offset() {}
      Offset(unsigned line, unsigned offset): line(line), offset(offset) {}
      bool operator==(const Offset &o) {return (line==o.line && offset==o.offset);}
      
      unsigned line;
      unsigned offset;
    };
    
    enum class Type {INSERT, REPLACE, ERASE};
    
    FixIt(Type type, const std::string &source, const std::pair<Offset, Offset> &offsets): 
      type(type), source(source), offsets(offsets) {}
    FixIt(const std::string &source, const std::pair<Offset, Offset> &offsets);
    
    std::string string();
    
    Type type;
    std::string source;
    std::pair<Offset, Offset> offsets;
  };

  class View : public Gsv::View {
  public:
    View(const boost::filesystem::path &file_path, const boost::filesystem::path &project_path, Glib::RefPtr<Gsv::Language> language);
    ~View();
    
    virtual void configure();
    
    void search_highlight(const std::string &text, bool case_sensitive, bool regex);
    std::function<void(int number)> update_search_occurrences;
    void search_forward();
    void search_backward();
    void replace_forward(const std::string &replacement);
    void replace_backward(const std::string &replacement);
    void replace_all(const std::string &replacement);
    
    void paste();
        
    boost::filesystem::path file_path;
    boost::filesystem::path project_path;
    Glib::RefPtr<Gsv::Language> language;
    
    std::function<std::pair<std::string, clang::Offset>()> get_declaration_location;
    std::function<void()> goto_method;
    std::function<Token()> get_token;
    std::function<std::vector<std::string>()> get_token_data;
    std::function<size_t(const Token &token, const std::string &text)> rename_similar_tokens;
    std::function<void()> goto_next_diagnostic;
    std::function<void()> apply_fix_its;
    
    std::function<void(View* view, const std::string &status)> on_update_status;
    std::function<void(View* view, const std::string &info)> on_update_info;
    void set_status(const std::string &status);
    void set_info(const std::string &info);
    std::string status;
    std::string info;
    
    void spellcheck();
    void remove_spellcheck_errors();
    void goto_next_spellcheck_error();
    
    void set_tab_char_and_size(char tab_char, unsigned tab_size);
    std::pair<char, unsigned> get_tab_char_and_size() {return {tab_char, tab_size};}
  protected:
    bool source_readable;
    Tooltips diagnostic_tooltips;
    Tooltips type_tooltips;
    virtual void show_diagnostic_tooltips(const Gdk::Rectangle &rectangle) {}
    virtual void show_type_tooltips(const Gdk::Rectangle &rectangle) {}
    gdouble on_motion_last_x;
    gdouble on_motion_last_y;
    sigc::connection delayed_tooltips_connection;
    void set_tooltip_events();
        
    std::string get_line(const Gtk::TextIter &iter);
    std::string get_line(Glib::RefPtr<Gtk::TextBuffer::Mark> mark);
    std::string get_line(int line_nr);
    std::string get_line();
    std::string get_line_before(const Gtk::TextIter &iter);
    std::string get_line_before(Glib::RefPtr<Gtk::TextBuffer::Mark> mark);
    std::string get_line_before();
    Gtk::TextIter get_tabs_end_iter(const Gtk::TextIter &iter);
    Gtk::TextIter get_tabs_end_iter(Glib::RefPtr<Gtk::TextBuffer::Mark> mark);
    Gtk::TextIter get_tabs_end_iter(int line_nr);
    Gtk::TextIter get_tabs_end_iter();
    
    bool find_start_of_closed_expression(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    bool find_open_expression_symbol(Gtk::TextIter iter, const Gtk::TextIter &until_iter, Gtk::TextIter &found_iter);
    bool find_right_bracket_forward(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    bool find_left_bracket_backward(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    
    bool on_key_press_event(GdkEventKey* key);
    bool on_button_press_event(GdkEventButton *event);
    
    std::pair<char, unsigned> find_tab_char_and_size();
    unsigned tab_size;
    char tab_char;
    std::string tab;
    
    bool spellcheck_all=false;
    std::unique_ptr<SelectionDialog> spellcheck_suggestions_dialog;
    bool spellcheck_suggestions_dialog_shown=false;
    bool last_keyval_is_backspace=false;
    bool last_keyval_is_return=false;
  private:
    GtkSourceSearchContext *search_context;
    GtkSourceSearchSettings *search_settings;
    static void search_occurrences_updated(GtkWidget* widget, GParamSpec* property, gpointer data);
        
    static AspellConfig* spellcheck_config;
    AspellCanHaveError *spellcheck_possible_err;
    AspellSpeller *spellcheck_checker;
    bool is_word_iter(const Gtk::TextIter& iter);
    std::pair<Gtk::TextIter, Gtk::TextIter> spellcheck_get_word(Gtk::TextIter iter);
    void spellcheck_word(const Gtk::TextIter& start, const Gtk::TextIter& end);
    std::vector<std::string> spellcheck_get_suggestions(const Gtk::TextIter& start, const Gtk::TextIter& end);
    sigc::connection delayed_spellcheck_suggestions_connection;
    sigc::connection delayed_spellcheck_error_clear;
    
    void spellcheck(const Gtk::TextIter& start, const Gtk::TextIter& end);
  };
  
  class GenericView : public View {
  private:
    class CompletionBuffer : public Gtk::TextBuffer {
    public:
      static Glib::RefPtr<CompletionBuffer> create() {return Glib::RefPtr<CompletionBuffer>(new CompletionBuffer());}
    };
  public:
    GenericView(const boost::filesystem::path &file_path, const boost::filesystem::path &project_path, Glib::RefPtr<Gsv::Language> language);
    
    void parse_language_file(Glib::RefPtr<CompletionBuffer> &completion_buffer, bool &has_context_class, const boost::property_tree::ptree &pt);
  };
  
  class ClangViewParse : public View {
  public:
    class TokenRange {
    public:
      TokenRange(std::pair<clang::Offset, clang::Offset> offsets, int kind):
        offsets(offsets), kind(kind) {}
      std::pair<clang::Offset, clang::Offset> offsets;
      int kind;
    };
    
    ClangViewParse(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language);
    ~ClangViewParse();
    void configure();
    
    void start_reparse();
    bool reparse_needed=false;
  protected:
    void init_parse();
    bool on_key_press_event(GdkEventKey* key);
    std::unique_ptr<clang::TranslationUnit> clang_tu;
    std::mutex parsing_mutex;
    std::unique_ptr<clang::Tokens> clang_tokens;
    sigc::connection delayed_reparse_connection;
    
    std::shared_ptr<Terminal::InProgress> parsing_in_progress;
    std::thread parse_thread;
    std::atomic<bool> parse_thread_stop;
    std::atomic<bool> parse_error;
    
    void show_diagnostic_tooltips(const Gdk::Rectangle &rectangle);
    void show_type_tooltips(const Gdk::Rectangle &rectangle);
    
    std::regex bracket_regex;
    std::regex no_bracket_statement_regex;
    std::regex no_bracket_no_para_statement_regex;
    
    std::set<int> diagnostic_offsets;
    std::vector<FixIt> fix_its;
  private:
    std::map<std::string, std::string> get_buffer_map() const;
    void update_syntax();
    std::set<std::string> last_syntax_tags;
    void update_diagnostics();
    
    static clang::Index clang_index;
    std::vector<std::string> get_compilation_commands();
    
    Glib::Dispatcher parse_done;
    Glib::Dispatcher parse_start;
    Glib::Dispatcher parse_fail;
    std::map<std::string, std::string> parse_thread_buffer_map;
    std::mutex parse_thread_buffer_map_mutex;
    std::atomic<bool> parse_thread_go;
    std::atomic<bool> parse_thread_mapped;
  };
    
  class ClangViewAutocomplete : public ClangViewParse {
  public:
    class AutoCompleteData {
    public:
      explicit AutoCompleteData(const std::vector<clang::CompletionChunk> &chunks) :
        chunks(chunks) { }
      std::vector<clang::CompletionChunk> chunks;
      std::string brief_comments;
    };
    
    ClangViewAutocomplete(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language);
    void async_delete();
    bool restart_parse();
  protected:
    bool on_key_press_event(GdkEventKey* key);
    std::thread autocomplete_thread;
  private:
    void start_autocomplete();
    void autocomplete();
    std::unique_ptr<CompletionDialog> completion_dialog;
    bool completion_dialog_shown=false;
    std::vector<AutoCompleteData> get_autocomplete_suggestions(int line_number, int column, std::map<std::string, std::string>& buffer_map);
    Glib::Dispatcher autocomplete_done;
    sigc::connection autocomplete_done_connection;
    Glib::Dispatcher autocomplete_fail;
    bool autocomplete_starting=false;
    std::atomic<bool> autocomplete_cancel_starting;
    guint last_keyval=0;
    std::string prefix;
    std::mutex prefix_mutex;
    
    Glib::Dispatcher do_delete_object;
    Glib::Dispatcher do_restart_parse;
    std::thread delete_thread;
    std::thread restart_parse_thread;
    bool restart_parse_running=false;
  };

  class ClangViewRefactor : public ClangViewAutocomplete {
  public:
    ClangViewRefactor(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language);
    ~ClangViewRefactor();
  private:
    std::list<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > similar_token_marks;
    void tag_similar_tokens(const Token &token);
    Glib::RefPtr<Gtk::TextTag> similar_tokens_tag;
    Token last_tagged_token;
    sigc::connection delayed_tag_similar_tokens_connection;
    std::unique_ptr<SelectionDialog> selection_dialog;
    bool renaming=false;
  };
  
  class ClangView : public ClangViewRefactor {
  public:
    ClangView(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language);
  };
};  // class Source
#endif  // JUCI_SOURCE_H_
