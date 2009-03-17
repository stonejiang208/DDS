/*
 * $Id$
 */

#ifndef GENERATOR_ERL_H
#define GENERATOR_ERL_H

#include "generator.h"

class generator_erl : public generator
{
public:
  generator_erl();

  ~generator_erl();

  bool generate_constant(AST_Constant* node);

  bool generate_enum(AST_Enum* node, std::vector<AST_EnumVal*>& v);

  bool generate_structure(AST_Structure* node, std::vector<AST_Field*>& v);
};

#endif /* GENERATOR_ERL_H */
