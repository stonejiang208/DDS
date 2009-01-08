/*
 * $Id$
 */

#include "ace/Log_Msg.h"
#include "ace/OS_NS_stdlib.h"
#include "ace/OS_Memory.h"

#include "ace_compat.h"
#include "ast_generator.h"
#include "ast_root.h"
#include "be_extern.h"
#include "be_visitor.h"
#include "global_extern.h"

int
BE_init(int &, char **)
{
#ifndef ACE_PRE_5_5
  ACE_NEW_RETURN(be_global, BE_GlobalData, -1);
#endif /* ACE_PRE_5_5 */
  return 0;
}

void
BE_post_init(BE_PI_CONST char **, long)
{
  idl_global->preserve_cpp_keywords(I_TRUE);
}

void
BE_version()
{
  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("TAO_IC_BE, version %s (Erlang Port Driver IDL BE)\n"),
             ACE_TEXT(TAO_IC_VERSION)));
}

void
BE_abort()
{
  // Force SIGABRT for debugging...
  ACE_ERROR((LM_ERROR, ACE_TEXT("%a\n")));
}

void
BE_cleanup()
{
  idl_global->destroy();
}

void
BE_produce()
{
  AST_Root *root = AST_Root::narrow_from_decl(idl_global->root());
  if (root == 0) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("%N:%l: BE_produce()")
               ACE_TEXT(" narrow_from_decl failed!\n")));
    BE_abort();
  }

  be_visitor visitor;
  if (root->ast_accept(&visitor) != 0) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("%N:%l: BE_produce()")
               ACE_TEXT(" ast_accept failed!\n")));
    BE_abort();
  }

  BE_cleanup();
}
