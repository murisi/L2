struct character {
	void *list_flag;
	char character;
};

union sexpr {
	void *list_flag;
	struct _list_ _list;
	struct character character;
};

bool is_lst(union sexpr *s) {
	return s->list_flag ? true : false;
}

list copy_sexpr_list(list l, region r) {
	list c = nil(r);
	union sexpr *s;
	foreach(s, l) {
		if(is_lst(s)) {
			append(copy_sexpr_list((list) s, r), &c, r);
		} else {
			union sexpr *t = region_alloc(r, sizeof(union sexpr));
			t->character = s->character;
			append(t, &c, r);
		}
	}
	return c;
}

bool char_equals(struct character *a, struct character *b) {
	return a->character == b->character ? true : false;
}

#define char_sexpr(str, ch) union sexpr _ ## str ## _ = { .character = { .list_flag = NULL, .character = ch }};

char_sexpr(exclamation_mark, '!'); char_sexpr(double_quotation, '"'); char_sexpr(number_sign, '#'); char_sexpr(dollar_sign, '$');
char_sexpr(percent, '%'); char_sexpr(ampersand, '&'); char_sexpr(apostrophe, '\''); char_sexpr(asterisk, '*');
char_sexpr(plus_sign, '+'); char_sexpr(comma, ','); char_sexpr(hyphen, '-'); char_sexpr(period, '.'); char_sexpr(slash, '/');
char_sexpr(0, '0'); char_sexpr(1, '1'); char_sexpr(2, '2'); char_sexpr(3, '3'); char_sexpr(4, '4'); char_sexpr(5, '5');
char_sexpr(6, '6'); char_sexpr(7, '7'); char_sexpr(8, '8'); char_sexpr(9, '9'); char_sexpr(colon, ':'); char_sexpr(semicolon, ';');
char_sexpr(less_than_sign, '<'); char_sexpr(equal_sign, '='); char_sexpr(greater_than_sign, '>'); char_sexpr(question_mark, '?');
char_sexpr(at_sign, '@'); char_sexpr(A, 'A'); char_sexpr(B, 'B'); char_sexpr(C, 'C'); char_sexpr(D, 'D'); char_sexpr(E, 'E');
char_sexpr(F, 'F'); char_sexpr(G, 'G'); char_sexpr(H, 'H'); char_sexpr(I, 'I'); char_sexpr(J, 'J'); char_sexpr(K, 'K');
char_sexpr(L, 'L'); char_sexpr(M, 'M'); char_sexpr(N, 'N'); char_sexpr(O, 'O'); char_sexpr(P, 'P'); char_sexpr(Q, 'Q');
char_sexpr(R, 'R'); char_sexpr(S, 'S'); char_sexpr(T, 'T'); char_sexpr(U, 'U'); char_sexpr(V, 'V'); char_sexpr(W, 'W');
char_sexpr(X, 'X'); char_sexpr(Y, 'Y'); char_sexpr(Z, 'Z'); char_sexpr(backslash, '\\'); char_sexpr(caret, '^');
char_sexpr(underscore, '_'); char_sexpr(backquote, '`'); char_sexpr(a, 'a'); char_sexpr(b, 'b'); char_sexpr(c, 'c');
char_sexpr(d, 'd'); char_sexpr(e, 'e'); char_sexpr(f, 'f'); char_sexpr(g, 'g'); char_sexpr(h, 'h'); char_sexpr(i, 'i');
char_sexpr(j, 'j'); char_sexpr(k, 'k'); char_sexpr(l, 'l'); char_sexpr(m, 'm'); char_sexpr(n, 'n'); char_sexpr(o, 'o');
char_sexpr(p, 'p'); char_sexpr(q, 'q'); char_sexpr(r, 'r'); char_sexpr(s, 's'); char_sexpr(t, 't'); char_sexpr(u, 'u');
char_sexpr(v, 'v'); char_sexpr(w, 'w'); char_sexpr(x, 'x'); char_sexpr(y, 'y'); char_sexpr(z, 'z'); char_sexpr(vertical_bar, '|');
char_sexpr(tilde, '~');

union sexpr *sexpr_symbols[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	&_exclamation_mark_,
	&_double_quotation_,
	&_number_sign_,
	&_dollar_sign_,
	&_percent_,
	&_ampersand_,
	&_apostrophe_,0,0,
	&_asterisk_,
	&_plus_sign_,
	&_comma_,
	&_hyphen_,
	&_period_,
	&_slash_,
	&_0_,
	&_1_,
	&_2_,
	&_3_,
	&_4_,
	&_5_,
	&_6_,
	&_7_,
	&_8_,
	&_9_,
	&_colon_,
	&_semicolon_,
	&_less_than_sign_,
	&_equal_sign_,
	&_greater_than_sign_,
	&_question_mark_,
	&_at_sign_,
	&_A_,
	&_B_,
	&_C_,
	&_D_,
	&_E_,
	&_F_,
	&_G_,
	&_H_,
	&_I_,
	&_J_,
	&_K_,
	&_L_,
	&_M_,
	&_N_,
	&_O_,
	&_P_,
	&_Q_,
	&_R_,
	&_S_,
	&_T_,
	&_U_,
	&_V_,
	&_W_,
	&_X_,
	&_Y_,
	&_Z_,0,
	&_backslash_,0,
	&_caret_,
	&_underscore_,
	&_backquote_,
	&_a_,
	&_b_,
	&_c_,
	&_d_,
	&_e_,
	&_f_,
	&_g_,
	&_h_,
	&_i_,
	&_j_,
	&_k_,
	&_l_,
	&_m_,
	&_n_,
	&_o_,
	&_p_,
	&_q_,
	&_r_,
	&_s_,
	&_t_,
	&_u_,
	&_v_,
	&_w_,
	&_x_,
	&_y_,
	&_z_,0,
	&_vertical_bar_,0,
	&_tilde_
};

union sexpr *build_character_sexpr(char d) {
	return sexpr_symbols[d];
}

list build_symbol_sexpr(char *str, region r) {
	list sexprs = nil(r);
	for(; *str; str++) {
		append(build_character_sexpr(*str), &sexprs, r);
	}
	return sexprs;
}

int after_leading_space(char *l2src, int l2src_sz, int *pos) {
	for(; *pos < l2src_sz && isspace(l2src[*pos]); (*pos)++);
	return l2src_sz - *pos;
}

list build_expr_list(char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler);

list build_sigil(char *sigil, char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	if(l2src_sz == *pos) {
		return build_symbol_sexpr(sigil, r);
	}
	char d = l2src[*pos];
	if(isspace(d) || d == ')' || d == '}' || d == ']' || d == '(' || d == '{' || d =='[') {
		return build_symbol_sexpr(sigil, r);
	} else {
		list sexprs = nil(r);
		append(build_symbol_sexpr(sigil, r), &sexprs, r);
		append(build_expr_list(l2src, l2src_sz, pos, r, handler), &sexprs, r);
		return sexprs;
	}
}

list build_list(char *primitive, char delimiter, char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	list sexprs = nil(r);
	append(build_symbol_sexpr(primitive, r), &sexprs, r);
	
	while(1) {
		int rem = after_leading_space(l2src, l2src_sz, pos);
		if(rem && l2src[*pos] == delimiter) {
			(*pos) ++;
			break;
		}
		append(build_expr_list(l2src, l2src_sz, pos, r, handler), &sexprs, r);
	}
	return sexprs;
}

list build_expr_list(char *l2src, int l2src_sz, int *pos, region r, jumpbuf *handler) {
	if(l2src_sz == *pos) {
		throw_unexpected_character(0, *pos, handler);
	}
	char c = l2src[*pos];
	if(isspace(c) || c == ')' || c == '}' || c == ']') {
		throw_unexpected_character(c, *pos, handler);
	}
	(*pos)++;
	if(c == '(') {
		return build_list("expression", ')', l2src, l2src_sz, pos, r, handler)->rst;
	} else if(c == '{') {
		return build_list("jump", '}', l2src, l2src_sz, pos, r, handler);
	} else if(c == '[') {
		return build_list("invoke", ']', l2src, l2src_sz, pos, r, handler);
	} else if(c == '$') {
		return build_sigil("$", l2src, l2src_sz, pos, r, handler);
	} else if(c == '&') {
		return build_sigil("&", l2src, l2src_sz, pos, r, handler);
	} else if(c == ',') {
		return build_sigil(",", l2src, l2src_sz, pos, r, handler);
	} else if(c == '`') {
		return build_sigil("`", l2src, l2src_sz, pos, r, handler);
	} else {
		list l = nil(r);
		do {
			append(build_character_sexpr(c), &l, r);
			if(*pos == l2src_sz) return l;
			c = l2src[(*pos)++];
		} while(!isspace(c) && c != '(' && c != ')' && c != '{' && c != '}' && c != '[' && c != ']');
		(*pos)--;
		return l;
	}
}

bool is_string(list d) {
	union sexpr *t;
	foreach(t, d) {
		if(is_lst(t)) {
			return false;
		}
	}
	return true;
}

char *to_string(list d, region r) {
	char *str = region_alloc(r, (length(d) + 1) * sizeof(char));
	int i = 0;
	
	union sexpr *t;
	foreach(t, d) {
		str[i++] = t->character.character;
	}
	str[i] = '\0';
	return str;
}

void print_expr_list(list d) {
	write_str(STDOUT, "(");
	if(!is_nil(d)) {
		union sexpr *_fst = d->fst;
		if(is_lst(_fst)) {
			print_expr_list((list) _fst);
		} else {
			write_char(STDOUT, _fst->character.character);
		}
		write_str(STDOUT, " . ");
		print_expr_list(d->rst);
	}
	write_str(STDOUT, ")");	
}
