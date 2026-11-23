#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <stdexcept>
#include <cctype>
using namespace std;

struct Token {
    string kind, text;
    int line, col, start, end, original;
};

struct Node {
    string kind, value, type;
    int line, col;
    vector<shared_ptr<Node>> children;
};

using Tree = shared_ptr<Node>;

Tree makeNode(string kind, const Token& t, string value = "") {
    auto n = make_shared<Node>();
    n->kind = kind;
    n->value = value;
    n->type = "";
    n->line = t.line;
    n->col = t.col;
    return n;
}

bool letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool digit(char c) {
    return c >= '0' && c <= '9';
}

struct Lexer {
    string text, file;
    int pos = 0, line = 1, col = 1;
    bool lastCR = false;
    vector<Token> tokens;
    vector<string> errors;
    set<string> keywords = {"array", "boolean", "char", "else", "false", "for", "function", "if",
                            "integer", "print", "return", "string", "true", "void", "while"};

    Lexer(string s, string name) : text(s), file(name) {}

    char peek(int step = 0) {
        int i = pos + step;
        return i >= 0 && i < (int)text.size() ? text[i] : '\0';
    }

    char take() {
        char c = peek();
        if (pos >= (int)text.size()) return '\0';
        pos++;
        if (c == '\r') {
            line++;
            col = 1;
            lastCR = true;
        } else if (c == '\n') {
            if (!lastCR) line++;
            col = 1;
            lastCR = false;
        } else {
            col++;
            lastCR = false;
        }
        return c;
    }

    void add(string kind, int start, int row, int column) {
        tokens.push_back({kind, text.substr(start, pos - start), row, column, start, pos, (int)tokens.size()});
    }

    void fail(int row, int column, string message) {
        errors.push_back(file + ":" + to_string(row) + ":" + to_string(column) + ": " + message);
    }

    void scan() {
        while (pos < (int)text.size()) {
            int start = pos, row = line, column = col;
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                while (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n') take();
                add("WS", start, row, column);
            } else if (letter(c) || c == '_') {
                take();
                while (letter(peek()) || digit(peek()) || peek() == '_') take();
                string word = text.substr(start, pos - start);
                add(keywords.count(word) ? "KW" : "ID", start, row, column);
                if (word.size() > 256) fail(row, column, "identifier is too long");
            } else if (digit(c)) {
                take();
                while (digit(peek())) take();
                add("INT", start, row, column);
            } else if (c == '/' && peek(1) == '/') {
                take(); take();
                while (peek() && peek() != '\n' && peek() != '\r') take();
                add("COMMENT", start, row, column);
            } else if (c == '/' && peek(1) == '*') {
                take(); take();
                bool closed = false;
                while (peek()) {
                    if (peek() == '*' && peek(1) == '/') {
                        take(); take();
                        closed = true;
                        break;
                    }
                    take();
                }
                add(closed ? "COMMENT" : "INVALID", start, row, column);
                if (!closed) fail(row, column, "unterminated block comment");
            } else if (c == '"' || c == '\'') {
                char quote = take();
                bool closed = false;
                int count = 0;
                while (peek() && peek() != '\n' && peek() != '\r') {
                    if (peek() == quote) {
                        take();
                        closed = true;
                        break;
                    }
                    if (peek() == '\\') {
                        take();
                        if (peek() && peek() != '\r' && peek() != '\n') take();
                    } else {
                        take();
                    }
                    count++;
                }
                bool valid = closed && (quote == '"' ? count <= 256 : count == 1);
                add(valid ? (quote == '"' ? "STRING" : "CHAR") : "INVALID", start, row, column);
                if (!valid) fail(row, column, quote == '"' ? "invalid or unterminated string" : "invalid character literal");
            } else {
                string pair = text.substr(pos, 2);
                if (pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" ||
                    pair == "&&" || pair == "||" || pair == "++" || pair == "--") {
                    take(); take();
                    add("OP", start, row, column);
                } else if (string("+-*/%^=!<>").find(c) != string::npos) {
                    take();
                    add("OP", start, row, column);
                } else if (string("()[]{};,: ").find(c) != string::npos && c != ' ') {
                    take();
                    add("PUNC", start, row, column);
                } else {
                    take();
                    add("INVALID", start, row, column);
                    fail(row, column, "unrecognized character");
                }
            }
        }
        tokens.push_back({"EOF", "", line, col, pos, pos, (int)tokens.size()});
    }
};

struct Parser {
    vector<Token> t;
    vector<string> errors;
    map<int, string> roles;
    string file;
    int p = 0;

    Parser(const vector<Token>& all, string f) : file(f) {
        for (auto x : all) {
            if (x.kind != "WS" && x.kind != "COMMENT") t.push_back(x);
        }
    }

    Token now() { return t[p]; }
    Token previous() { return t[p > 0 ? p - 1 : 0]; }
    bool at(string s) { return now().text == s; }
    bool done() { return now().kind == "EOF"; }
    Token next() { Token x = now(); if (!done()) p++; return x; }
    bool accept(string s) { if (at(s)) { next(); return true; } return false; }

    void error(string message) {
        errors.push_back(file + ":" + to_string(now().line) + ":" + to_string(now().col) + ": " + message);
    }

    Token need(string s) {
        if (at(s)) return next();
        error("expected '" + s + "'");
        throw runtime_error("syntax");
    }

    Token identifier() {
        if (now().kind == "ID") return next();
        error("expected identifier");
        throw runtime_error("syntax");
    }

    void recover() {
        if (accept(";")) return;
        while (!done() && !at(";") && !at("}") && !at("if") && !at("return") && !at("for") && !at("print")) next();
        accept(";");
    }

    Tree type(bool allowVoid = false) {
        Token tok = now();
        if (accept("array")) {
            auto n = makeNode("Type", tok, "array");
            need("[");
            if (!at("]")) n->children.push_back(expression());
            need("]");
            n->children.push_back(type(false));
            return n;
        }
        if (at("integer") || at("boolean") || at("char") || at("string") || (allowVoid && at("void"))) {
            auto n = makeNode("Type", next(), tok.text);
            return n;
        }
        error("expected type");
        throw runtime_error("syntax");
    }

    Tree declaration() {
        Token name = identifier();
        need(":");
        if (accept("function")) {
            auto n = makeNode("FuncDecl", name, name.text);
            roles[name.original] = "fn";
            n->children.push_back(type(true));
            need("(");
            if (!at(")")) {
                do {
                    Token arg = identifier();
                    need(":");
                    auto param = makeNode("Param", arg, arg.text);
                    roles[arg.original] = "var";
                    param->children.push_back(type());
                    n->children.push_back(param);
                } while (accept(","));
            }
            need(")");
            if (!accept(";")) {
                need("=");
                n->children.push_back(block());
            }
            return n;
        }
        auto n = makeNode("VarDecl", name, name.text);
        roles[name.original] = "var";
        n->children.push_back(type());
        if (accept("=")) n->children.push_back(expression());
        need(";");
        return n;
    }

    Tree block() {
        Token brace = need("{");
        auto n = makeNode("Block", brace);
        while (!done() && !at("}")) {
            int before = p;
            try {
                if (now().kind == "ID" && p + 1 < (int)t.size() && t[p + 1].text == ":")
                    n->children.push_back(declaration());
                else
                    n->children.push_back(statement());
            } catch (const runtime_error&) {
                recover();
            }
            if (p == before && !done()) next();
        }
        need("}");
        return n;
    }

    Tree statement() {
        Token tok = now();
        if (at("{")) return block();
        if (accept("return")) {
            auto n = makeNode("ReturnStmt", tok);
            if (!at(";")) n->children.push_back(expression());
            need(";");
            return n;
        }
        if (accept("print")) {
            auto n = makeNode("PrintStmt", tok);
            if (!at(";")) {
                do { n->children.push_back(expression()); } while (accept(","));
            }
            need(";");
            return n;
        }
        if (accept("if")) {
            auto n = makeNode("IfStmt", tok);
            need("(");
            n->children.push_back(expression());
            need(")");
            n->children.push_back(statement());
            if (accept("else")) n->children.push_back(statement());
            return n;
        }
        if (accept("for")) {
            auto n = makeNode("ForStmt", tok);
            need("(");
            if (!at(";")) n->children.push_back(expression());
            else n->children.push_back(makeNode("Empty", now()));
            need(";");
            if (!at(";")) n->children.push_back(expression());
            else n->children.push_back(makeNode("Empty", now()));
            need(";");
            if (!at(")")) n->children.push_back(expression());
            else n->children.push_back(makeNode("Empty", now()));
            need(")");
            n->children.push_back(statement());
            return n;
        }
        if (accept(";")) return makeNode("EmptyStmt", tok);
        auto n = makeNode("ExprStmt", tok);
        n->children.push_back(expression());
        need(";");
        return n;
    }

    int precedence(string op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "==" || op == "!=") return 3;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
        if (op == "+" || op == "-") return 5;
        if (op == "*" || op == "/" || op == "%") return 6;
        if (op == "^") return 7;
        return 0;
    }

    Tree expression() {
        auto left = binary(1);
        if (accept("=")) {
            Token op = previous();
            auto n = makeNode("AssignExpr", op, "=");
            n->children = {left, expression()};
            return n;
        }
        return left;
    }

    Tree binary(int level) {
        auto left = unary();
        while (precedence(now().text) >= level) {
            Token op = next();
            int pr = precedence(op.text);
            auto n = makeNode("BinaryExpr", op, op.text);
            n->children.push_back(left);
            n->children.push_back(binary(pr + (op.text == "^" ? 0 : 1)));
            left = n;
        }
        return left;
    }

    Tree unary() {
        if (at("-") || at("!")) {
            Token op = next();
            auto n = makeNode("UnaryExpr", op, op.text);
            n->children.push_back(unary());
            return n;
        }
        return postfix();
    }

    Tree postfix() {
        auto left = primary();
        while (true) {
            if (accept("(")) {
                Token tok = previous();
                auto n = makeNode("CallExpr", tok);
                if (left->kind == "Identifier") {
                    n->value = left->value;
                    for (auto &x : t) {
                        if (x.line == left->line && x.col == left->col && x.kind == "ID") roles[x.original] = "fn";
                    }
                }
                n->children.push_back(left);
                if (!at(")")) {
                    do { n->children.push_back(expression()); } while (accept(","));
                }
                need(")");
                left = n;
            } else if (accept("[")) {
                Token tok = previous();
                auto n = makeNode("IndexExpr", tok);
                n->children.push_back(left);
                n->children.push_back(expression());
                need("]");
                left = n;
            } else if (at("++") || at("--")) {
                Token tok = next();
                auto n = makeNode("PostfixExpr", tok, tok.text);
                n->children.push_back(left);
                left = n;
            } else break;
        }
        return left;
    }

    Tree primary() {
        Token tok = now();
        if (tok.kind == "ID") {
            next();
            roles[tok.original] = "var";
            return makeNode("Identifier", tok, tok.text);
        }
        if (tok.kind == "INT" || tok.kind == "STRING" || tok.kind == "CHAR" ||
            at("true") || at("false")) {
            next();
            return makeNode("Literal", tok, tok.text);
        }
        if (accept("(")) {
            auto n = expression();
            need(")");
            return n;
        }
        if (accept("{")) {
            auto n = makeNode("ArrayInit", tok);
            if (!at("}")) {
                do {
                    if (at("}")) break;
                    n->children.push_back(expression());
                } while (accept(","));
            }
            need("}");
            return n;
        }
        error("expected expression");
        throw runtime_error("syntax");
    }

    Tree parse() {
        Token first = now();
        auto root = makeNode("Program", first);
        while (!done()) {
            int before = p;
            try { root->children.push_back(declaration()); }
            catch (const runtime_error&) { recover(); }
            if (p == before && !done()) next();
        }
        return root;
    }
};

void showTree(ostream& out, const Tree& n, int depth = 0) {
    out << string(depth * 2, ' ') << n->kind;
    if (!n->value.empty()) out << " " << n->value;
    out << " (" << n->line << ":" << n->col << ", type=" << (n->type.empty() ? "null" : n->type) << ")\n";
    for (auto child : n->children) showTree(out, child, depth + 1);
}

string escapeHtml(string s) {
    string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else if (c == '\'') out += "&#39;";
        else out += c;
    }
    return out;
}

string colorClass(const Token& tok, const map<int,string>& roles) {
    if (tok.kind == "KW") {
        if (tok.text == "true" || tok.text == "false") return "lit";
        if (tok.text == "integer" || tok.text == "boolean" || tok.text == "char" ||
            tok.text == "string" || tok.text == "void" || tok.text == "array") return "type";
        return "kw";
    }
    if (tok.kind == "INT" || tok.kind == "CHAR" || tok.kind == "STRING") return "lit";
    if (tok.kind == "COMMENT") return "comment";
    if (tok.kind == "OP") return "op";
    if (tok.kind == "INVALID") return "bad";
    if (tok.kind == "ID") {
        auto it = roles.find(tok.original);
        return it == roles.end() ? "var" : it->second;
    }
    return "";
}

void writeHtml(const vector<Token>& tokens, const map<int,string>& roles, string path) {
    ofstream out(path);
    out << "<!doctype html>\n<html><head><meta charset=\"UTF-8\">\n";
    out << "<title>B-Minor</title><style>body{background:#f7f8fa;padding:26px;}";
    out << "pre{font:15px/1.5 Consolas,monospace;background:white;padding:22px;white-space:pre-wrap;border:1px solid #ddd;}";
    out << ".kw{color:#1854c1;font-weight:bold}.type{color:#7d3ca8}.lit{color:#b05216}.fn{color:#176c68;font-weight:bold}";
    out << ".var{color:#262626}.comment{color:#718080}.op{color:#ab2673}.bad{background:#ffd7d7;color:#c20f0f}</style>";
    out << "</head><body><pre>";
    for (const auto& t : tokens) {
        if (t.kind == "EOF") continue;
        string kind = colorClass(t, roles);
        if (!kind.empty()) out << "<span class=\"" << kind << "\">";
        out << escapeHtml(t.text);
        if (!kind.empty()) out << "</span>";
    }
    out << "</pre></body></html>\n";
}

void printAnsi(const vector<Token>& tokens, const map<int,string>& roles) {
    map<string,string> color = {{"kw", "\033[1;34m"}, {"type", "\033[1;35m"}, {"lit", "\033[33m"},
        {"fn", "\033[1;36m"}, {"var", "\033[37m"}, {"comment", "\033[90m"},
        {"op", "\033[35m"}, {"bad", "\033[1;31m"}};
    for (const auto& t : tokens) {
        if (t.kind == "EOF") continue;
        string kind = colorClass(t, roles);
        if (color.count(kind)) cout << color[kind];
        cout << t.text;
        if (color.count(kind)) cout << "\033[0m";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: ./bminor source.bminor\n";
        return 1;
    }
    ifstream in(argv[1], ios::binary);
    if (!in) {
    cerr << "Cannot open file: " << argv[1] << "\n";
    return 1;
}
    string source((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    Lexer lexer(source, argv[1]);
    lexer.scan();
    Parser parser(lexer.tokens, argv[1]);
    Tree tree = parser.parse();

    ofstream tokenFile("tokens.txt");
    for (auto x : lexer.tokens) {
        if (x.kind != "EOF" && x.kind != "WS")
            tokenFile << x.line << ":" << x.col << " " << x.kind << " " << x.text << "\n";
    }
    ofstream astFile("ast.txt");
    showTree(astFile, tree);
    writeHtml(lexer.tokens, parser.roles, "highlighted.html");
    printAnsi(lexer.tokens, parser.roles);

    ofstream errorFile("errors.txt");
    for (auto x : lexer.errors) { cerr << x << "\n"; errorFile << x << "\n"; }
    for (auto x : parser.errors) { cerr << x << "\n"; errorFile << x << "\n"; }
    return (lexer.errors.empty() && parser.errors.empty()) ? 0 : 2;
}
