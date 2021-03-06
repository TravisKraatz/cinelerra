/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                       Copyright (c) 1996,1997                         */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                      Author :  Alan W Black                           */
/*                      Date   :  August 1996                            */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Phrase break prediction                                               */
/*                                                                       */
/*=======================================================================*/
#include <cstdio>
#include "festival.h"
#include "modules.h"

static void phrasing_none(EST_Utterance *u);
static void phrasing_by_cart(EST_Utterance *u);
static void phrasing_by_probmodels(EST_Utterance *u);
static void phrasing_by_fa(EST_Utterance *u);
static EST_VTCandidate *bb_candlist(EST_Item *s,EST_Features &f);
static EST_VTPath *bb_npath(EST_VTPath *p,EST_VTCandidate *c,EST_Features &f);
static double find_b_prob(EST_VTPath *p,int n,int *state);

// Used in various default value cases
static int B_word = 0;
static int BB_word = 0;
static int NB_word = 0;
static int pos_p_start_tag = 0;
static int pos_pp_start_tag = 0;
static int pos_n_start_tag = 0;

static double gscale_s = 1.0;
static double gscale_p = 0.0;
static EST_Ngrammar *bb_ngram = 0;
static EST_Ngrammar *bb_pos_ngram = 0;
static LISP bb_tags = NIL;
static LISP pos_map = NIL;
static LISP phrase_type_tree = NIL;

/* Interslice */
static EST_Track *bb_track = 0;

LISP FT_Classic_Phrasify_Utt(LISP utt)
{
    // Predict and add phrasing to an utterance
    EST_Utterance *u = get_c_utt(utt);
    LISP phrase_method = ft_get_param("Phrase_Method");

    *cdebug << "Phrasify module\n";

    if (u->relation_present("Phrase"))
	return utt;               // already specified
    else if (phrase_method == NIL)
	phrasing_none(u);  // all one phrase
    else if (streq("prob_models",get_c_string(phrase_method)))
	phrasing_by_probmodels(u);
    else if (streq("cart_tree",get_c_string(phrase_method)))
	phrasing_by_cart(u);
    else if (streq("forced_align",get_c_string(phrase_method)))
	phrasing_by_fa(u);
    else
    {
	cerr << "PHRASIFY: unknown phrase method \"" <<
	    get_c_string(phrase_method) << endl;
	festival_error();
    }

    return utt;
}

static void phrasing_none(EST_Utterance *u)
{
    // All in a single phrase
    EST_Item *w,*phr=0;

    u->create_relation("Phrase");

    for (w=u->relation("Word")->first(); w != 0; w = next(w))
    {
	if (phr == 0)
	    phr = add_phrase(u);
	append_daughter(phr,"Phrase",w);
	if (next(w) == 0)
	{
	    w->set("pbreak","B");
	    phr->set_name("4");
	    phr = 0;
	}
    }
    
}

static void phrasing_by_cart(EST_Utterance *u)
{
    EST_Item *w,*phr=0;
    LISP tree;
    EST_Val pbreak;

    u->create_relation("Phrase");
    tree = siod_get_lval("phrase_cart_tree","no phrase cart tree");

    for (w=u->relation("Word")->first(); w != 0; w = next(w))
    {
	if (phr == 0)
	    phr = add_phrase(u);
	append_daughter(phr,"Phrase",w);
	pbreak = wagon_predict(w,tree);
	w->set("pbreak",pbreak.string());
	if ((pbreak == "B") || (pbreak == "BB"))
	{
	    phr->set_name((EST_String)pbreak);
	    phr = 0;
	}
    }
    
}

static void pbyp_get_params(LISP params)
{
    EST_String bb_pos_name,bb_pos_filename,bb_name,bb_break_filename;
    EST_String bb_track_name;
    LISP l1;

    bb_pos_name = get_param_str("pos_ngram_name",params,"");
    bb_pos_filename = get_param_str("pos_ngram_filename",params,"");
    if ((bb_pos_ngram = get_ngram(bb_pos_name,bb_pos_filename)) == 0)
    {
	cerr << "PHRASIFY: no ngram called \"" <<
	    bb_pos_name << "\" defined." << endl;
	festival_error();
    }

    gscale_s = get_param_float("gram_scale_s",params,1.0);
    gscale_p = get_param_float("gram_scale_p",params,0.0);
    pos_map = get_param_lisp("pos_map",params,NIL);

    bb_name = get_param_str("break_ngram_name",params,"");
    bb_break_filename = get_param_str("break_ngram_filename",params,"");
    if ((bb_ngram = get_ngram(bb_name,bb_break_filename)) == 0)
    {
	cerr << "PHRASIFY: no ngram called \"" <<
	    bb_name << "\" defined." << endl;
	festival_error();
    }
    bb_tags = get_param_lisp("break_tags",params,NIL);
    phrase_type_tree = get_param_lisp("phrase_type_tree",params,NIL);

    bb_track_name = get_param_str("break_track_name",params,"");
    if (bb_track_name != "")
    {
	if (bb_track)
	    delete bb_track;
	bb_track = new EST_Track;
	if (bb_track->load(bb_track_name) != format_ok)
	{
	    delete bb_track;
	    cerr << "PHRASE: failed to load FA track " << 
		bb_track_name << endl;
	    festival_error();
	} 
    }

    l1 = siod_get_lval("pos_p_start_tag",NULL);
    if (l1 != NIL) 
	pos_p_start_tag = bb_pos_ngram->get_vocab_word(get_c_string(l1));
    l1 = siod_get_lval("pos_pp_start_tag",NULL);
    if (l1 != NIL) 
	pos_pp_start_tag = bb_pos_ngram->get_vocab_word(get_c_string(l1));
    l1 = siod_get_lval("pos_n_start_tag",NULL);
    if (l1 != NIL) 
	pos_n_start_tag = bb_pos_ngram->get_vocab_word(get_c_string(l1));
}

static void phrasing_by_probmodels(EST_Utterance *u)
{
    // Predict phrasing using POS and prob models of B distribution 
    EST_Item *w,*phr=0;
    EST_String pbreak;
    int num_states;

    pbyp_get_params(siod_get_lval("phr_break_params",NULL));
    gc_protect(&bb_tags);

    for (w=u->relation("Word")->first(); w != 0; w = next(w))
    {   // Set up tag index for pos ngram
	EST_String lpos = map_pos(pos_map,w->f("pos").string());
	w->set("phr_pos",lpos);
	w->set("pos_index",bb_pos_ngram->get_vocab_word(lpos));
    }
    B_word = bb_ngram->get_vocab_word("B");
    NB_word = bb_ngram->get_vocab_word("NB");
    BB_word = bb_ngram->get_vocab_word("BB");

    num_states = bb_ngram->num_states();
    EST_Viterbi_Decoder v(bb_candlist,bb_npath,num_states);

    v.initialise(u->relation("Word"));
    v.search();
    v.result("pbreak_index");

    // Given predicted break, go through and add phrases 
    u->create_relation("Phrase");
    for (w=u->relation("Word")->first(); w != 0; w = next(w))
    {
	w->set("pbreak",bb_ngram->
		 get_vocab_word(w->f("pbreak_index").Int()));
	if (phr == 0)
	    phr = add_phrase(u);
	append_daughter(phr,"Phrase",w);
	if (phrase_type_tree != NIL)
	{
	    EST_Val npbreak = wagon_predict(w,phrase_type_tree);
	    w->set("pbreak",npbreak.string());  // may reset to BB
	}
	pbreak = (EST_String)w->f("pbreak");
	if (pbreak == "B")
	    w->set("blevel",3);
	else if (pbreak == "mB")
	    w->set("blevel",2);
	if ((pbreak == "B") || (pbreak == "BB") || (pbreak == "mB"))
	{
	    phr->set_name((EST_String)pbreak);
	    phr = 0;
	}
    }

    gc_unprotect(&bb_tags);
    bb_tags = NIL;
}

static EST_VTCandidate *bb_candlist(EST_Item *s,EST_Features &f)
{
    // Find candidates with a priori probabilities
    EST_IVector window(bb_pos_ngram->order());
    (void)f;
    int tag;

    if (bb_pos_ngram->order() == 4)
    {
	window[1] = s->I("pos_index",0);
	if (prev(s) != 0)
	    window[0] = prev(s)->I("pos_index",0);
	else
	    window[0] = pos_p_start_tag;
	if (next(s) != 0)
	    window[2] = next(s)->I("pos_index",0);
	else
	    window[2] = pos_n_start_tag;
    }
    else if (bb_pos_ngram->order() == 3)
    {
	window[0] = s->I("pos_index",0);
	if (next(s) != 0)
	    window[1] = next(s)->I("pos_index",0);
	else
	    window[1] = pos_n_start_tag;
    }
    else if (bb_pos_ngram->order() == 5)
    {   // This is specific for some set of pos tagsets
	window[2] = s->I("pos_index",0);
	if (prev(s) != 0)
	{
	    window[1] = prev(s)->I("pos_index",0);
	}
	else
	{
	    window[1] = pos_p_start_tag;
	}
	if (next(s) != 0)
	{
	    window[3] = next(s)->I("pos_index",0);
	    if (next(next(s)) != 0)
		window[0] = next(next(s))->I("pos_index",0);
	    else
		window[0] = 0;
	}
	else
	{
	    window[3] = pos_n_start_tag;
	    window[0] = 0;
	}
    }
    else
    {
	cerr << "PHRASIFY: can't deal with ngram of size " <<
	    bb_pos_ngram->order() << endl;
	festival_error();
    }
    double prob=1.0;
    EST_VTCandidate *all_c = 0;
    EST_Val labelled_brk = ffeature(s,"R:Token.parent.pbreak");

    if ((labelled_brk != "0") &&
	(ffeature(s,"R:Token.n.name") == "0")) // last word in token
    {   // there is a labelled break on the token so respect it 
	EST_VTCandidate *c = new EST_VTCandidate;
	c->s = s;
	c->name = bb_ngram->get_vocab_word(labelled_brk.string());
	c->score = log(0.95);  // very very likely, but not absolute
	c->next = all_c;
	all_c = c;  // but then if you give only one option ...
    }
    else if (next(s) == 0)  // end of utterances so force a break
    {   
	EST_VTCandidate *c = new EST_VTCandidate;
	c->s = s;
	c->name = B_word;
	c->score = log(0.95);  // very very likely, but not absolute
	c->next = all_c;
	all_c = c;
    }
    else if (s->name() == ".end_utt")
    {   // This is a quick check to see if forcing "." to B is worth it
	EST_VTCandidate *c = new EST_VTCandidate;
	c->s = s;
	c->name = B_word;
	c->score = log(0.95);  // very very likely, but not absolute
	c->next = all_c;
	all_c = c;
    }
    else if (siod_get_lval("break_non_bayes",NULL) != NIL)
    {
        /* This uses the "wrong" formula to extract the  probability      */
        /* Extract P(B | context) rather than P(context | B) as below     */
        /* This gives worse results as well as not following Bayes        */
        /* equations                                                      */
	EST_VTCandidate *c;
	LISP l;
	for (l=bb_tags; l != 0; l=cdr(l))
	{
	    c = new EST_VTCandidate;
	    c->s = s;
	    tag = bb_ngram->get_vocab_word(get_c_string(car(l)));
	    c->name = tag;
	    window[bb_pos_ngram->order()-1] = tag;
	    const EST_DiscreteProbDistribution &pd = 
		bb_pos_ngram->prob_dist(window);
	    if (pd.samples() == 0)
	    {
		if (tag == B_word)
		    prob = 0.2;
		else
		    prob = 0.8;
	    }
	    else
		prob = pd.probability(tag);
	    if (prob == 0)
		c->score = log(0.0000001);
	    else
		c->score = log(prob);
	    c->next = all_c;
	    all_c = c;
	}
    }
    else
    {   // Standard Bayes model
	EST_VTCandidate *c;
	LISP l;
	for (l=bb_tags; l != 0; l=cdr(l))
	{
	    c = new EST_VTCandidate;
	    c->s = s;
	    tag = bb_ngram->get_vocab_word(get_c_string(car(l)));
	    c->name = tag;
	    window[bb_pos_ngram->order()-1] = tag;
	    prob = bb_pos_ngram->reverse_probability(window);

	    // If this word came from inside a token reduce the
	    // probability of a break
 	    if ((ffeature(s,"R:Token.n.name") != "0") &&
		((s->as_relation("Token")->first()->length()) < 7))
 	    {
 		float weight = ffeature(s,"pbreak_scale");
 		if (weight == 0) weight = 0.5;
 		if (tag == B_word)
 		    prob *= weight;
 		else
 		    prob = 1.0-((1.0-prob)*weight);
 	    }
	    if (prob == 0)
		c->score = log(0.0000001);
	    else
		c->score = log(prob);
	    s->set("phrase_score",c->score);
	    c->next = all_c;
	    all_c = c;
	}
    }

    return all_c;
}

static EST_VTPath *bb_npath(EST_VTPath *p,EST_VTCandidate *c,EST_Features &f)
{
    EST_VTPath *np = new EST_VTPath;
    (void)f;
//    static EST_String lscorename("lscore");
    double prob;
    double lprob,lang_prob;
    
    np->c = c;
    np->from = p;
    int n = c->name.Int();
    prob = find_b_prob(p,n,&np->state);
    if (np->state == -1)
	prob = find_b_prob(p,n,&np->state);
    if (prob == 0)
	lprob = log(0.00000001);
    else
	lprob = log(prob);
    
    lang_prob = (1.0 * c->score) + gscale_p;
    lang_prob = c->score;

//    np->set_feature(lscorename,lang_prob+lprob);
    if (p==0)
	np->score = (lang_prob+lprob);
    else
	np->score = (lang_prob+lprob) + p->score;
    
    return np;
}

static double find_b_prob(EST_VTPath *p,int n,int *state)
{
    int oldstate=0;
    double prob;

    if (p == 0)
    {
	int order = bb_ngram->order();
	EST_IVector window(order);
	int i;
	window.a_no_check(order-1) = n;
	window.a_no_check(order-2) = B_word;
	for (i=order-3; i>=0; i--)
	    window.a_no_check(i) = NB_word;
	oldstate = bb_ngram->find_state_id(window);
    }
    else
	oldstate = p->state;
    const EST_DiscreteProbDistribution &pd = bb_ngram->prob_dist(oldstate);
    if (pd.samples() == 0)
	prob = 0;
    else
	prob = (double)pd.probability(n);
    // This is too specific
    if (n == B_word)
	prob *= gscale_s;
    *state = bb_ngram->find_next_state_id(oldstate,n);

    return prob;

}

/* Part of Interslice */

static double find_b_faprob(EST_VTPath *p,int n,int *state)
{
    int oldstate=0;
    int i,j;
    EST_VTPath *d;
    double prob;
    double atime, wtime, wstddev=0, z;
    static int ATOTH_BREAK=2;
    static int ATOTH_NBREAK=1;

    if (p == 0)
    {
	oldstate = 0;
    }
    else
	oldstate = p->state;

/*    if (streq("of",
	      ( p && p->c && p->c->s ? (const char *)ffeature(p->c->s,"name").String() : "null")))
	      printf("ding\n"); */

    // Prob is the prob that time since last break could be 
    // generated by the duration model 

    // Skip over break if we're at one
    for (i = oldstate; i < bb_track->num_frames(); i++)
    {
	if (bb_track->a(i,0) == ATOTH_NBREAK)
	    break;
    }
    
    // time since last break in words with std
    for (wstddev=wtime=0.0,d=p; d ; d=d->from)
    {
	wtime += ffeature(d->c->s,"word_duration").Float();
	wstddev += ffeature(d->c->s,"lisp_word_stddev").Float();
	if (bb_track->a(d->state,0) == ATOTH_BREAK)
	    break;
    }

    // time since last break in acoustics
    for (atime=0.01,j=i; j>0; j--)
    {
	if (bb_track->a(j,0) == ATOTH_BREAK)
	    break;
	atime += bb_track->t(j) - ( i == 0 ? 0 : bb_track->t(j-1));
    }

    // Find best state for give time 
    if (wstddev == 0)
	i++;
    else if (n == B_word)
    {   /* cost of having a break here */
	/* extend acoustics until next break */
	for (; i < bb_track->num_frames(); i++)
	{
	    if (bb_track->a(i,0) == ATOTH_BREAK)
		break;
	    atime += bb_track->t(i) - ( i == 0 ? 0 : bb_track->t(i-1));
	}
	z = fabs((atime-wtime)/wstddev);
    }
    else
    {   /* cost of having a non-break here */
	for ( i++,z = fabs((atime-wtime)/wstddev); 
	      (i < bb_track->num_frames()) && (bb_track->a(i,0) == ATOTH_NBREAK);
	      i++)
	{
	    atime += bb_track->t(i) - ( i == 0 ? 0 : bb_track->t(i-1));
/*	    printf("better atime %f wtime %f wstddev %f z %f new z %f\n",
		   atime,wtime,wstddev,z,
		   fabs((atime-wtime)/wstddev)); */
	    if (fabs((atime-wtime)/wstddev) > (float)z)
		break;
	    else
		z = fabs((atime-wtime)/wstddev);
	}
    }


    if (d && d->c && d->c->s && (d->c->s->next()->next()) == NULL)
    {   /* must be in final state */
	printf("must be in final state\n");
	if (i != bb_track->num_frames())
	    z = 0.000001;
    }

    if (z == 0)
	printf("z == 0");

    /* number hack */
    prob = (2.0 - z)/2.0;
/*    prob = z; */
    if (prob < 0.000001)
	prob = 0.000001;
    else if (prob > 0.999999)
	prob = 0.999999;

    // prob of atime given (wtime,wstddev)
    printf("%d %d %f %f %f %f %s %s %f\n",oldstate,i,atime,wtime,wstddev,z,
	   ( p && p->c && p->c->s ? (const char *)ffeature(p->c->s,"name").String() : "null"),
	   ( n == B_word ? "B" : "NB"),
	   prob
	   );
    if (i >=  bb_track->num_frames())
	i =  bb_track->num_frames() - 1;
    *state = i;

    return prob;

}

static EST_VTPath *bb_fapath(EST_VTPath *p,EST_VTCandidate *c,EST_Features &f)
{
    EST_VTPath *np = new EST_VTPath;
    (void)f;
//    static EST_String lscorename("lscore");
    double prob;
    double lprob,lang_prob;
    
    np->c = c;
    np->from = p;
    int n = c->name.Int();
    prob = find_b_faprob(p,n,&np->state);
    if (prob == 0)
	lprob = log(0.00000001);
    else
	lprob = log(prob);
    
    lang_prob = (1.0 * c->score) + gscale_p;
    lang_prob = c->score;

//    np->set_feature(lscorename,lang_prob+lprob);
    if (p==0)
	np->score = (lang_prob+lprob);
    else
	np->score = (lang_prob+lprob) + p->score;
    
    return np;
}

static void phrasing_by_fa(EST_Utterance *u)
{
    // Predict phrasing using POS and prob models of B distribution 
    EST_Item *w,*phr=0;
    EST_String pbreak;
    int num_states;

    pbyp_get_params(siod_get_lval("phr_break_params",NULL));
    gc_protect(&bb_tags);

    for (w=u->relation("Word")->first(); w != 0; w = next(w))
    {   // Set up tag index for pos ngram
	EST_String lpos = map_pos(pos_map,w->f("pos").string());
	w->set("phr_pos",lpos);
	w->set("pos_index",bb_pos_ngram->get_vocab_word(lpos));
    }

    num_states = bb_track->num_frames();

    EST_Viterbi_Decoder v(bb_candlist,bb_fapath,num_states);

    v.initialise(u->relation("Word"));
    v.search();
    v.result("pbreak_index");

    // Given predicted break, go through and add phrases 
    u->create_relation("Phrase");
    for (w=u->relation("Word")->first(); w != 0; w = next(w))
    {
	w->set("pbreak",bb_ngram->
		 get_vocab_word(w->f("pbreak_index").Int()));
	if (phr == 0)
	    phr = add_phrase(u);
	append_daughter(phr,"Phrase",w);
	if (phrase_type_tree != NIL)
	{
	    EST_Val npbreak = wagon_predict(w,phrase_type_tree);
	    w->set("pbreak",npbreak.string());  // may reset to BB
	}
	pbreak = (EST_String)w->f("pbreak");
	if (pbreak == "B")
	    w->set("blevel",3);
	else if (pbreak == "mB")
	    w->set("blevel",2);
	if ((pbreak == "B") || (pbreak == "BB") || (pbreak == "mB"))
	{
	    phr->set_name((EST_String)pbreak);
	    phr = 0;
	}
    }

    gc_unprotect(&bb_tags);
    bb_tags = NIL;
}


EST_Item *add_phrase(EST_Utterance *u)
{
    EST_Item *item = u->relation("Phrase")->append();

    item->set_name("phrase");
    
    return item;

}

