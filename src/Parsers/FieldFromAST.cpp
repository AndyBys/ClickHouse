#include <Parsers/FieldFromAST.h>
#include <Disks/getOrCreateDiskFromAST.h>
#include <Parsers/formatAST.h>
#include <Parsers/ASTIdentifier.h>
#include <Parsers/ASTLiteral.h>
#include <Parsers/ASTFunction.h>
#include <Parsers/isDiskFunction.h>
#include <Common/assert_cast.h>


namespace DB
{
namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
    extern const int BAD_ARGUMENTS;
}

Field createFieldFromAST(ASTPtr ast)
{
    return CustomType(std::make_shared<FieldFromASTImpl>(ast));
}

[[noreturn]] void FieldFromASTImpl::throwNotImplemented(std::string_view method) const
{
    throw Exception(ErrorCodes::LOGICAL_ERROR, "Method {} not implemented for {}", method, getTypeName());
}

bool FieldFromASTImpl::isSecret() const
{
    return isDiskFunction(ast);
}

String FieldFromASTImpl::toString(bool show_secrets) const
{
    if (!show_secrets && isDiskFunction(ast))
    {
        auto hidden = ast->clone();
        const auto & disk_function = assert_cast<const ASTFunction &>(*hidden);
        const auto * disk_function_args_expr = assert_cast<const ASTExpressionList *>(disk_function.arguments.get());
        const auto & disk_function_args = disk_function_args_expr->children;

        auto is_secret_arg = [](const std::string & arg_name)
        {
            return arg_name != "type";
        };

        for (const auto & arg : disk_function_args)
        {
            auto * setting_function = arg->as<ASTFunction>();
            if (!setting_function || setting_function->name != "equals")
                throw Exception(ErrorCodes::BAD_ARGUMENTS, "Bad format: expected equals function");

            auto * function_args_expr = assert_cast<ASTExpressionList *>(setting_function->arguments.get());
            if (!function_args_expr)
                throw Exception(ErrorCodes::BAD_ARGUMENTS, "Bad format: expected arguments");

            auto & function_args = function_args_expr->children;
            if (function_args.empty())
                throw Exception(ErrorCodes::BAD_ARGUMENTS, "Bad format: expected non zero number of arguments");

            auto * key_identifier = function_args[0]->as<ASTIdentifier>();
            if (!key_identifier)
                throw Exception(ErrorCodes::BAD_ARGUMENTS, "Bad format: expected Identifier");

            const std::string & key = key_identifier->name();
            if (is_secret_arg(key))
                function_args[1] = std::make_shared<ASTLiteral>("[HIDDEN]");
        }
        return serializeAST(*hidden);
    }

    return serializeAST(*ast);
}

}
