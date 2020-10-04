bool is_var(list s) {
  return !is_nil(s) && s->rst == s ? true : false;
}

bool var_equals(list a, list b) {
  return a == b ? true : false;
}

void set_val(list var, list val) {
  var->fst = val;
}

list get_val(list var) {
  return var->fst;
}

struct character {
  unsigned char character;
  void *flag;
};

bool char_equals(struct character *a, struct character *b) {
  return a->character == b->character;
}

#define char_init(ch) {{ .character = ch, .flag = NULL }}

struct character chars[][1] = {
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, char_init('!'),
  char_init('"'), char_init('#'), char_init('$'), char_init('%'), char_init('&'), char_init('\''), {0}, {0}, char_init('*'),
  char_init('+'), char_init(','), char_init('-'), char_init('.'), char_init('/'), char_init('0'), char_init('1'),
  char_init('2'), char_init('3'), char_init('4'), char_init('5'), char_init('6'), char_init('7'), char_init('8'),
  char_init('9'), {0}, {0}, {0}, char_init('='), {0}, char_init('?'),
  char_init('@'), char_init('A'), char_init('B'), char_init('C'), char_init('D'), char_init('E'), char_init('F'),
  char_init('G'), char_init('H'), char_init('I'), char_init('J'), char_init('K'), char_init('L'), char_init('M'),
  char_init('N'), char_init('O'), char_init('P'), char_init('Q'), char_init('R'), char_init('S'), char_init('T'),
  char_init('U'), char_init('V'), char_init('W'), char_init('X'), char_init('Y'), char_init('Z'), {0}, char_init('\\'),
  {0}, char_init('^'), char_init('_'), char_init('`'), char_init('a'), char_init('b'), char_init('c'), char_init('d'),
  char_init('e'), char_init('f'), char_init('g'), char_init('h'), char_init('i'), char_init('j'), char_init('k'),
  char_init('l'), char_init('m'), char_init('n'), char_init('o'), char_init('p'), char_init('q'), char_init('r'),
  char_init('s'), char_init('t'), char_init('u'), char_init('v'), char_init('w'), char_init('x'), char_init('y'),
  char_init('z'), {0}, char_init('|'), {0}, char_init('~'), {0}
};

#define _char(d) (chars[d])

list build_token(unsigned char *str, region r) {
  list sexprs = nil;
  for(; *str; str++) {
    append(_char(*str), &sexprs, r);
  }
  return sexprs;
}

unsigned long gentok_counter = 0;

list gentok(region r) {
  unsigned char buf[21];
  return build_token(ulong_to_str(++gentok_counter, buf), r);
}

/*
 * Grammar being lexed:
 * program = <ignore> (<fragment> <ignore>)*
 * fragment = <fragment1> | <sublist>
 * sublist = <fragment1> (<ignore> ':' <ignore> <fragment1>)+
 * fragment1 = <fragment2> | <subsublist>
 * subsublist = <fragment2> (<ignore> ';' <ignore> <fragment2>)+
 * fragment2 = <token> | <list> | <clist> | <slist>
 * list = '(' <ignore> (<fragment> <ignore>)* ')'
 * clist = '{' <ignore> (<fragment> <ignore>)* '}'
 * slist = '[' <ignore> (<fragment> <ignore>)* ']'
 * token = <character>+
 * <ignore> = (<comment> | <whitespace>)*
 * <comment> = '<' <ignore> (<fragment> <ignore>)* '>'
 */

list read_fragment(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler);
int read_ignore(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler);

int read_comment(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
  if(l2src_sz == *pos) {
    throw_unexpected_character(0, *pos, handler);
  } else {
    unsigned char c = l2src[*pos];
    if(c == '<') {
      (*pos) ++;
      list sexprs = nil;
      for(;;) {
        if(read_ignore(l2src, l2src_sz, pos, r, handler) && l2src[*pos] == '>') {
          (*pos) ++;
          return l2src_sz - *pos;
        }
        append(read_fragment(l2src, l2src_sz, pos, r, handler), &sexprs, r);
      }
    } else {
      throw_unexpected_character(c, *pos, handler);
    }
  }
}

int read_ignore(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
  while(*pos < l2src_sz) {
    if(isspace(l2src[*pos])) {
      (*pos)++;
    } else if(l2src[*pos] == '<') {
      read_comment(l2src, l2src_sz, pos, r, handler);
    } else break;
  }
  return l2src_sz - *pos;
}

list read_token(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
  if(l2src_sz == *pos) {
    throw_unexpected_character(0, *pos, handler);
  } else {
    unsigned char c = l2src[*pos];
    if(isspace(c) || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ';' || c == '<' || c == '>') {
      throw_unexpected_character(c, *pos, handler);
    } else {
      (*pos) ++;
      list l = nil;
      do {
        append(_char(c), &l, r);
        if(*pos == l2src_sz) {
          return l;
        }
        c = l2src[(*pos)++];
      } while(!isspace(c) && c != '(' && c != ')' && c != '{' && c != '}' && c != '[' && c != ']' && c != ':' && c != ';' && c != '<' && c != '>');
      (*pos)--;
      return l;
    }
  }
}

list read_list(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
  if(l2src_sz == *pos) {
    throw_unexpected_character(0, *pos, handler);
  } else {
    unsigned char c = l2src[*pos];
    if(c == '(' || c == '{' || c == '[') {
      (*pos) ++;
      unsigned char delimiter;
      list sexprs = nil;
      if(c == '{') {
        append(build_token("jump", r), &sexprs, r);
        delimiter = '}';
      } else if(c == '[') {
        append(build_token("invoke", r), &sexprs, r);
        delimiter = ']';
      } else {
        delimiter = ')';
      }
      for(;;) {
        if(read_ignore(l2src, l2src_sz, pos, r, handler) && l2src[*pos] == delimiter) {
          (*pos) ++;
          return sexprs;
        }
        append(read_fragment(l2src, l2src_sz, pos, r, handler), &sexprs, r);
      }
    } else {
      throw_unexpected_character(c, *pos, handler);
    }
  }
}

list read_fragment2(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
  if(*pos == l2src_sz) {
    throw_unexpected_character(0, *pos, handler);
  } else {
    unsigned char c = l2src[*pos];
    if(c == '(' || c == '{' || c == '[') {
      return read_list(l2src, l2src_sz, pos, r, handler);
    } else {
      return read_token(l2src, l2src_sz, pos, r, handler);
    }
  }
}

list read_fragment1(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
  list sexprs = nil;
  append(read_fragment2(l2src, l2src_sz, pos, r, handler), &sexprs, r);
  while(read_ignore(l2src, l2src_sz, pos, r, handler)) {
    unsigned char c = l2src[*pos];
    if(c == ';') {
      (*pos) ++;
      read_ignore(l2src, l2src_sz, pos, r, handler);
      append(read_fragment2(l2src, l2src_sz, pos, r, handler), &sexprs, r);
    } else {
      break;
    }
  }
  if(is_nil(sexprs->rst)) {
    return sexprs->fst;
  } else {
    return sexprs;
  }
}

list read_fragment(unsigned char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
  list sexprs = nil;
  append(read_fragment1(l2src, l2src_sz, pos, r, handler), &sexprs, r);
  while(read_ignore(l2src, l2src_sz, pos, r, handler)) {
    unsigned char c = l2src[*pos];
    if(c == ':') {
      (*pos) ++;
      read_ignore(l2src, l2src_sz, pos, r, handler);
      append(read_fragment1(l2src, l2src_sz, pos, r, handler), &sexprs, r);
    } else {
      break;
    }
  }
  if(is_nil(sexprs->rst)) {
    return sexprs->fst;
  } else {
    return sexprs;
  }
}

bool is_token(list d) {
  return !is_var(d) && length(d) && !((struct character *) d->fst)->flag;
}

unsigned char *to_string(list d, region r) {
  unsigned char *str = region_alloc(r, (length(d) + 1) * sizeof(unsigned char));
  int i = 0;
  
  struct character *t;
  foreach(t, d) {
    str[i++] = t->character;
  }
  str[i] = '\0';
  return str;
}

bool token_equals(list a, list b) {
  if(length(a) != length(b)) {
    return false;
  }
  struct character *x, *y;
  foreachzipped(x, y, a, b) {
    if(x->character != y->character) {
      return false;
    }
  }
  return true;
}

list var(region reg) {
  list ret = region_alloc(reg, sizeof(struct _list_));
  ret->fst = ret;
  ret->rst = ret;
  return ret;
}

list evaluate(list val) {
  list next_val;
  while(is_var(val) && val != (next_val = get_val(val))) {
    val = next_val;
  }
  return val;
}

list recursive_evaluate(list fragment, region reg) {
  list d = evaluate(fragment);
  if(is_var(d) || is_token(d)) {
    return d;
  } else {
    list res = nil, t;
    foreach(t, d) {
      append(recursive_evaluate(t, reg), &res, reg);
    }
    return res;
  }
}

list copy_fragment(list l, list *old_vars, list *new_vars, region r) {
  if(is_var(l)) {
    list ov, nv;
    foreachzipped(ov, nv, *old_vars, *new_vars) {
      if(ov == l) {
        return nv;
      }
    }
    prepend(l, old_vars, r);
    prepend(var(r), new_vars, r);
    return (*new_vars)->fst;
  } else if(is_token(l)) {
    list c = nil;
    struct character *s;
    foreach(s, l) {
      append(_char(s->character), &c, r);
    }
    return c;
  } else {
    list c = nil, s;
    foreach(s, l) {
      append(copy_fragment(s, old_vars, new_vars, r), &c, r);
    }
    return c;
  }
}

void print_fragment(list d) {
  if(is_var(d)) {
    write_str(STDOUT, "!");
    write_ulong(STDOUT, (long) d % 8192);
  } else if(is_token(d)) {
    struct character *t;
    foreach(t, d) {
      write_char(STDOUT, t->character);
    }
  } else {
    write_str(STDOUT, "(");
    list t;
    foreach(t, d) {
      print_fragment((list) t);
      write_str(STDOUT, " ");
    }
    write_str(STDOUT, ")"); 
  }
}

