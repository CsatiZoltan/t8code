/*
  This file is part of t8code.
  t8code is a C library to manage a collection (a forest) of multiple
  connected adaptive space-trees of general element classes in parallel.

  Copyright (C) 2015 the developers

  t8code is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  t8code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with t8code; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <gtest/gtest.h>
//#include <iostream>
#include <t8.h>
#include <t8_eclass.h>
#include <t8_cmesh.h>
#include <t8_cmesh/t8_cmesh_examples.h>
#include <t8_forest/t8_forest.h>
//#include <t8_vec.h>
#include <t8_schemes/t8_default/t8_default_cxx.hxx>
#include <t8_forest/t8_forest_private.h>
#include <t8_forest/t8_forest_iterate.h>
#include "t8_cmesh/t8_cmesh_testcases.h"

//#include <t8_forest/t8_forest_partition.h>


/* *INDENT-OFF* */
class forest_iterate:public testing::TestWithParam <int> {
protected:
  void SetUp () override {
    cmesh_id = GetParam();

    forest =
        t8_forest_new_uniform (t8_test_create_cmesh (cmesh_id), 
                               t8_scheme_new_default_cxx (),
                               3, 0, sc_MPI_COMM_WORLD);

    //adapt_callbacks = NULL;
  }

  void TearDown () override {
    t8_forest_unref (&forest);
    t8_forest_unref (&forest_adapt);
    if (adapt_callbacks != NULL) {
      T8_FREE (adapt_callbacks);
    }
  }

  int                 cmesh_id;
  t8_forest_t         forest;
  t8_forest_t         forest_adapt;
  int                 *adapt_callbacks;
};
/* *INDENT-ON* */


struct t8_return_data
{
  int                 *callbacks;
};

void
t8_forest_replace (t8_forest_t forest_old,
                   t8_forest_t forest_new,
                   t8_locidx_t which_tree,
                   t8_eclass_scheme_c *ts,
                   int refine,
                   int num_outgoing,
                   t8_locidx_t first_outgoing,
                   int num_incoming,
                   t8_locidx_t first_incoming)
{
  /* The new forest contains the callback returns of the old forest */
  struct t8_return_data *adapt_data = (struct t8_return_data *) t8_forest_get_user_data (forest_new);
  T8_ASSERT (adapt_data != NULL);

  /* element index of old forest */
  t8_locidx_t elidx_old = first_outgoing;
  t8_locidx_t elidx_new = first_incoming;
  for (t8_locidx_t tidx = 0; tidx < which_tree; tidx++)
  {
    elidx_new += t8_forest_get_tree_num_elements (forest_new, tidx);
    elidx_old += t8_forest_get_tree_num_elements (forest_old, tidx);
  }

  //printf ("which_tree: %i \n", which_tree);
  //printf ("elidx_new:  %i \n", elidx_new);
  //printf ("elidx_old:  %i \n\n", elidx_old);

  ASSERT_EQ (adapt_data->callbacks[elidx_old], refine);

  //T8_ASSERT (first_outgoing == elidx_old);
  //T8_ASSERT (refine == -2 ? true : first_incoming == elidx_new);

  if (refine == 0) {
    ASSERT_EQ (num_outgoing, 1);
    ASSERT_EQ (num_incoming, 1);
  }
  if (refine == -1) {
    ASSERT_EQ (num_incoming, 1);

    /* check family */
    t8_element_t *parent = t8_forest_get_element_in_tree (forest_new, which_tree, first_incoming);
    t8_element_t *child;
    t8_element_t *parent_compare;
    ts->t8_element_new (1, &parent_compare);
    int family_size = 1;
    t8_locidx_t tree_num_elements_old = t8_forest_get_tree_num_elements (forest_old ,which_tree);
    for (t8_locidx_t elidx = 1; elidx < ts->t8_element_num_children (parent)
          && elidx + first_outgoing < tree_num_elements_old; elidx++) {
      child = t8_forest_get_element_in_tree (forest_old, which_tree, first_outgoing + elidx);
      ts->t8_element_parent (child, parent_compare);
      if (!ts->t8_element_compare (parent, parent_compare)) {
        family_size++;
      }
    }

    ts->t8_element_destroy (1, &parent_compare);
    ASSERT_EQ (num_outgoing, family_size);
    /* end check family */
    for (t8_locidx_t i = 1; i < num_outgoing; i++) {
      ASSERT_EQ (adapt_data->callbacks[elidx_old + i], -3);
    }
  }
  if (refine == -2) {
    //T8_ASSERT (forest_new->incomplete_trees == 1);
    ASSERT_EQ (num_outgoing, 1);
    ASSERT_EQ (num_incoming, 0);
  }
  if (refine == 1) {
    ASSERT_EQ (num_outgoing, 1);
    t8_element_t *element = t8_forest_get_element_in_tree (forest_old, which_tree, first_outgoing);
    const t8_locidx_t family_size = ts->t8_element_num_children (element);
    ASSERT_EQ (num_incoming, family_size);
  }

}


int
t8_adapt_callback (t8_forest_t forest,
                   t8_forest_t forest_from,
                   t8_locidx_t which_tree,
                   t8_locidx_t lelement_id,
                   t8_eclass_scheme_c *ts,
                   const int is_family,
                   const int num_elements, t8_element_t *elements[])
{
  struct t8_return_data *return_data = (struct t8_return_data *) t8_forest_get_user_data (forest);
  T8_ASSERT (return_data != NULL);

  int return_val = lelement_id % 20;
  if (return_val < 5) {
    return_val = -2;
  }
  else if (return_val < 10) {
    if (is_family) {
      return_val = -1;
    }
    else {
      return_val = 0;
    }
  }
  else if (return_val < 15) {
    return_val = 0;
  }
  else {
    return_val = 1;
  }
  ASSERT_TRUE (-3 < return_val);
  ASSERT_TRUE (return_val < 2);

  t8_locidx_t lelement_id_forest = lelement_id;
  for (t8_locidx_t tidx = 0; tidx < which_tree; tidx++)
  {
    lelement_id_forest += t8_forest_get_tree_num_elements (forest_from, tidx);
  }
  return_data->callbacks[lelement_id_forest] = return_val;
  return return_val;
}



t8_forest_t
t8_adapt_forest (t8_forest_t forest_from,
                 t8_forest_adapt_t adapt_fn,
                 void *user_data)
{
  t8_forest_t         forest_new;

  t8_forest_init (&forest_new);
  t8_forest_set_adapt (forest_new, forest_from, adapt_fn, 0);
  if (user_data != NULL) {
    t8_forest_set_user_data (forest_new, user_data);
  }
  t8_forest_commit (forest_new);

  return forest_new;
}



TEST_P (forest_iterate, test_iterate_replace)
{

  t8_locidx_t num_elements = t8_forest_get_local_num_elements (forest);

  adapt_callbacks = T8_ALLOC (int, num_elements);

  for (t8_locidx_t elidx = 0; elidx < num_elements; elidx++) {
    adapt_callbacks[elidx] = -3;
  }

  struct t8_return_data data {
    adapt_callbacks
  };

  t8_forest_ref (forest);
  forest_adapt = t8_adapt_forest (forest, t8_adapt_callback, &data);

  for (t8_locidx_t elidx = 0; elidx < num_elements; elidx++) {
   //printf ("%i: %i \n",elidx, adapt_callbacks[elidx]);
  }
  //t8_forest_write_vtk (forest, "output_forest");
  //t8_forest_write_vtk (forest_adapt, "output_forest_adapt");

  t8_forest_iterate_replace (forest_adapt, forest, t8_forest_replace);

}
//t8_get_number_of_all_testcases ()
/* *INDENT-OFF* */
INSTANTIATE_TEST_SUITE_P (t8_gtest_iterate_replace, forest_iterate, testing::Range(100, 101));
/* *INDENT-ON* */

