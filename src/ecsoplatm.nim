
import macros

#design an ecs type for generation reference

# type Foo* = object
#     a: int

# type Bar* = object
#     b: float


# type
#   Components = enum
#     foo,
#     bar

#   ECS* {.dumpAstGen.} = ref object
    # foo: seq[Foo]
    # bar: seq[Bar]


# macro setComponents(names, types: varargs[untyped]) =
# macro declECS*(ecsname, components: varargs[untyped]name, typ: untyped) =
#   result = quote do:
#     type
#       `ecsname` = ref object
#         `name`: `typ`

  # echo result.repr

macro declECS*(ecsname, components: varargs[untyped]) =
  result = quote do:
    type
      `ecsname` = ref object

  for i in 0..<components.len:
    let
      name = components[i]
      typ = components[i + 1]
      result.append(

        nnkIdentDefs.newTree(
          newIdentNode(name),
          nnkBracketExpr.newTree(
            newIdentNode("seq"),
            newIdentNode(typ)
          ),
          newEmptyNode()
        ),

      )

  echo result.repr


# macro setComponents() =
#   nnkTypeDef.newTree(
#     nnkPragmaExpr.newTree(
#       nnkPostfix.newTree(
#         newIdentNode("*"),
#         newIdentNode("ECS")
#       ),
#       nnkPragma.newTree(
#       )
#     ),
#     newEmptyNode(),
#     nnkRefTy.newTree(
#       nnkObjectTy.newTree(
#         newEmptyNode(),
#         newEmptyNode(),
#         nnkRecList.newTree(
#           nnkIdentDefs.newTree(
#             newIdentNode("foo"),
#             nnkBracketExpr.newTree(
#               newIdentNode("seq"),
#               newIdentNode("Foo")
#             ),
#             newEmptyNode()
#           ),
#           nnkIdentDefs.newTree(
#             newIdentNode("bar"),
#             nnkBracketExpr.newTree(
#               newIdentNode("seq"),
#               newIdentNode("Bar")
#             ),
#             newEmptyNode()
#           )
#         )
#       )
#     )
#   )



# # macro generateECS(components: varargs[untyped]):
# # #   type
# # #     ECS = ref object
# #       # x: int


# # from nim glm source as example...
# # template vecGen(U:untyped, V:typed) =
# #   ## ``U`` suffix
# #   ## ``V`` valType
# #   type
# #     `Vec4 U`* {.inject.} = Vec4[V]
# #     `Vec3 U`* {.inject.} = Vec3[V]
# #     `Vec2 U`* {.inject.} = Vec2[V]
