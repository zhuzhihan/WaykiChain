#include <wasm/exceptions.hpp>
#include <wasm/abi_serializer.hpp>
#include <wasm/types/name.hpp>
#include <wasm/types/symbol.hpp>
#include <wasm/types/asset.hpp>
#include <wasm/types/varint.hpp>
#include <wasm/wasm_log.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include "json/json_spirit_writer.h"

using namespace boost;
using namespace wasm;
using namespace std;

namespace wasm {

    //const size_t abi_serializer::max_recursion_depth;

    using boost::algorithm::ends_with;
    using std::string;

    template<typename T>
    inline json_spirit::Value variant_from_stream( wasm::datastream<const char *> &ds ) {
        T temp;
        ds >> temp;

        json_spirit::Value var;
        wasm::to_variant(temp, var);

        return var;
    }

    template<typename T>
    auto pack_unpack() {
        return std::make_pair<abi_serializer::unpack_function, abi_serializer::pack_function>(
                []( wasm::datastream<const char *> &ds, bool is_array, bool is_optional ) -> json_spirit::Value {
                    if (is_array)
                        return variant_from_stream<vector < T>>
                    (ds);
                    else if (is_optional)
                    return variant_from_stream<optional < T>>
                    (ds);
                    return variant_from_stream<T>(ds);
                },
                []( const json_spirit::Value &var, wasm::datastream<char *> &ds, bool is_array, bool is_optional ) {
                    if (is_array) {
                        vector <T> ts;
                        wasm::from_variant(var, ts);
                        ds << ts;
                    } else if (is_optional) {
                        optional <T> opt;
                        wasm::from_variant(var, opt);
                        ds << opt;
                    } else {
                        T t;
                        wasm::from_variant(var, t);
                        ds << t;
                    }
                }
        );
    }

    abi_serializer::abi_serializer( const abi_def &abi, const microseconds &max_serialization_time ) {
        configure_built_in_types();
        set_abi(abi, max_serialization_time);
    }

    // void abi_serializer::add_specialized_unpack_pack( const string& name,
    //                                                   std::pair<abi_serializer::unpack_function, abi_serializer::pack_function> unpack_pack ) {
    //    built_in_types[name] = std::move( unpack_pack );
    // }

    void abi_serializer::configure_built_in_types() {

        built_in_types.emplace("bool", pack_unpack<uint8_t>());
        built_in_types.emplace("int8", pack_unpack<int8_t>());
        built_in_types.emplace("uint8", pack_unpack<uint8_t>());
        built_in_types.emplace("int16", pack_unpack<int16_t>());
        built_in_types.emplace("uint16", pack_unpack<uint16_t>());
        built_in_types.emplace("int32", pack_unpack<int32_t>());
        built_in_types.emplace("uint32", pack_unpack<uint32_t>());
        built_in_types.emplace("int64", pack_unpack<int64_t>());
        built_in_types.emplace("uint64", pack_unpack<uint64_t>());
        // built_in_types.emplace("int128",                    pack_unpack<int128_t>());
        // built_in_types.emplace("uint128",                   pack_unpack<uint128_t>());
        built_in_types.emplace("varint32", pack_unpack<wasm::signed_int>());
        built_in_types.emplace("varuint32", pack_unpack<wasm::unsigned_int>());

        // TODO: Add proper support for floating point types. For now this is good enough.
        built_in_types.emplace("float32", pack_unpack<float>());
        built_in_types.emplace("float64", pack_unpack<double>());
        // built_in_types.emplace("float128",                  pack_unpack<uint128_t>());

        // built_in_types.emplace("time_point",                pack_unpack<std::time_point>());
        // built_in_types.emplace("time_point_sec",            pack_unpack<std::time_point_sec>());
        // built_in_types.emplace("block_timestamp_type",      pack_unpack<block_timestamp_type>());

        built_in_types.emplace("table_name", pack_unpack<name>());
        built_in_types.emplace("action_name", pack_unpack<name>());
        built_in_types.emplace("name", pack_unpack<name>());

        built_in_types.emplace("bytes", pack_unpack<bytes>());
        built_in_types.emplace("string", pack_unpack<string>());

        // built_in_types.emplace("checksum160",               pack_unpack<checksum160_type>());
        // built_in_types.emplace("checksum256",               pack_unpack<checksum256_type>());
        // built_in_types.emplace("checksum512",               pack_unpack<checksum512_type>());

        // built_in_types.emplace("public_key",                pack_unpack<public_key_type>());
        // built_in_types.emplace("signature",                 pack_unpack<signature_type>());

        built_in_types.emplace("symbol", pack_unpack<symbol>());
        built_in_types.emplace("symbol_code", pack_unpack<symbol_code>());
        built_in_types.emplace("asset", pack_unpack<asset>());
        // built_in_types.emplace("extended_asset",            pack_unpack<extended_asset>());
    }

    void abi_serializer::set_abi( const abi_def &abi, const microseconds &max_serialization_time ) {
        wasm::abi_traverse_context ctx(max_serialization_time);

        WASM_ASSERT(boost::starts_with(abi.version, "wasm::abi/1."), unsupport_abi_version_exception,
                    "ABI has an unsupported version");
        typedefs.clear();
        structs.clear();
        actions.clear();
        tables.clear();
        // error_messages.clear();
        // variants.clear();
        for (const auto &st : abi.structs)
        {
            structs[st.name] = st;
            //WASM_TRACE("%s", st.name.c_str())
        }

        //WASM_TRACE("%d", abi.structs.size())

        for (const auto &td : abi.types) {
            WASM_ASSERT(_is_type(td.type, ctx), invalid_type_inside_abi, "Invalid type %s",
                        td.type.c_str());

            //std::cout << "type:" << td.type << std::endl;

            WASM_ASSERT(!_is_type(td.new_type_name, ctx), duplicate_abi_def_exception,
                        "Type %s already exists", td.new_type_name.c_str());

            //std::cout << "new_type_name:" << td.new_type_name << std::endl;

            typedefs[td.new_type_name] = td.type;
        }
        for (const auto &a : abi.actions)
            actions[a.name] = a.type;

        for (const auto &t : abi.tables)
            tables[t.name] = t.type;

        for (const auto &e : abi.error_messages)
            error_messages[e.error_code] = e.error_msg;

        // for( const auto& v : abi.variants.value )
        //    variants[v.name] = v;

        /**
         *  The ABI vector may contain duplicates which would make it
         *  an invalid ABI
         */
        WASM_ASSERT(typedefs.size() == abi.types.size(), duplicate_abi_def_exception,
                    "Duplicate type definition detected");
        WASM_ASSERT(structs.size() == abi.structs.size(), duplicate_abi_def_exception,
                    "Duplicate struct definition detected");
        WASM_ASSERT(actions.size() == abi.actions.size(), duplicate_abi_def_exception,
                    "Duplicate action definition detected");
        WASM_ASSERT(tables.size() == abi.tables.size(), duplicate_abi_def_exception,
                    "Duplicate table definition detected");
        //WASM_ASSERT( error_messages.size() == abi.error_messages.size(), duplicate_abi_err_msg_def_exception, "duplicate error message definition detected" );
        //WASM_ASSERT( variants.size() == abi.variants.value.size(), duplicate_abi_variant_def_exception, "duplicate variant definition detected" );
        validate(ctx);
    }

    bool abi_serializer::is_builtin_type( const type_name &type ) const {
        return built_in_types.find(type) != built_in_types.end();
    }

    bool abi_serializer::is_integer( const type_name &type ) const {
        string stype = type;
        return boost::starts_with(stype, "uint") || boost::starts_with(stype, "int");
    }

    int abi_serializer::get_integer_size( const type_name &type ) const {
        string stype = type;

        WASM_ASSERT(is_integer(type), invalid_type_inside_abi, "%s is not an integer type",
                    stype.data());

        if (boost::starts_with(stype, "uint")) {
            return boost::lexical_cast<int>(stype.substr(4));
        } else {
            return boost::lexical_cast<int>(stype.substr(3));
        }
    }

    bool abi_serializer::is_struct( const type_name &type ) const {
        return structs.find(resolve_type(type)) != structs.end();
    }

    bool abi_serializer::is_array( const type_name &type ) const {
        return boost::ends_with(string(type), "[]");
    }

    bool abi_serializer::is_optional( const type_name &type ) const {
        return boost::ends_with(string(type), "?");
    }

    bool abi_serializer::is_type( const type_name &type, const microseconds &max_serialization_time ) const {
        wasm::abi_traverse_context ctx(max_serialization_time);
        return _is_type(type, ctx);
    }

    type_name abi_serializer::fundamental_type( const type_name &type ) const {
        if (is_array(type)) {
            return type_name(string(type).substr(0, type.size() - 2));
        } else if (is_optional(type)) {
            return type_name(string(type).substr(0, type.size() - 1));
        } else {
            return type;
        }
    }

    type_name abi_serializer::_remove_bin_extension( const type_name &type ) {
        if (boost::ends_with(type, "$"))
            return type.substr(0, type.size() - 1);
        else
            return type;
    }

    bool abi_serializer::_is_type( const type_name &rtype, wasm::abi_traverse_context &ctx ) const {

        //WASM_TRACE("%s", rtype.c_str())
        ctx.check_deadline();
        auto type = fundamental_type(rtype);
        //WASM_TRACE("%s", type.c_str())

        if (built_in_types.find(type) != built_in_types.end()) return true;
        if (typedefs.find(type) != typedefs.end()) return _is_type(typedefs.find(type)->second, ctx);

        //WASM_TRACE("%s", type.c_str())
        if (structs.find(type) != structs.end()) return true;
        //WASM_TRACE("%s", type.c_str())
        //if( variants.find(type) != variants.end() ) return true;
        return false;
    }

    const struct_def &abi_serializer::get_struct( const type_name &type ) const {
        auto itr = structs.find(resolve_type(type));

        WASM_ASSERT(itr != structs.end(), invalid_type_inside_abi, "Unknown struct %s",
                    type.data());

        return itr->second;
    }

    type_name abi_serializer::resolve_type( const type_name &type ) const {
        auto itr = typedefs.find(type);
        if (itr != typedefs.end()) {
            for (auto i = typedefs.size(); i > 0; --i) { // avoid infinite recursion
                const type_name &t = itr->second;
                itr = typedefs.find(t);
                if (itr == typedefs.end()) return t;
            }
        }
        return type;
    }

    json_spirit::Value abi_serializer::_binary_to_variant( const type_name &type, wasm::datastream<const char *> &ds,
                                                           wasm::abi_traverse_context &ctx ) const {
        ctx.check_deadline();
        //std::cout << "[recursion_depth:" << ctx.recursion_depth <<" type:" << type << "]"<< std::endl ;
        ctx.recursion_depth++;

        type_name rtype = resolve_type(type);
        auto ftype = fundamental_type(rtype);
        auto btype = built_in_types.find(ftype);
        if (btype != built_in_types.end()) {
            try {
                return btype->second.first(ds, is_array(rtype), is_optional(rtype));
            }
            WASM_RETHROW_EXCEPTIONS(unpack_exception, "Unable to unpack type '%s' ", rtype.c_str())
        }

        auto s_itr = structs.end();
        if (is_array(rtype)) {
            wasm::unsigned_int size;
            try {
                ds >> size;
            }
            WASM_RETHROW_EXCEPTIONS(unpack_exception, "Unable to unpack size of array '%s' ",
                                    rtype.c_str())

            WASM_ASSERT(size < max_array_size_for_abi, array_size_exceeds_exception, "Array size %u is biger than max %d", size.value, 
                            max_array_size_for_abi); 

            //std::cout << "size:---------------------------------------------- " << size.value << std::endl ;
            json_spirit::Array vars;
            for (decltype(size.value) i = 0; i < size; ++i) {
                auto v = _binary_to_variant(ftype, ds, ctx);
                //std::cout << "size:---------------------------------------------- " << ftype.data() << std::endl ;
                WASM_ASSERT(!v.is_null(), unpack_exception, "Invalid packed array '%s'",
                            rtype.c_str());

                vars.emplace_back(std::move(v));
            }
            return json_spirit::Value(std::move(vars));
        } else if (is_optional(rtype)) {
            char flag;
            try {
                ds >> flag;
            }

            WASM_RETHROW_EXCEPTIONS(unpack_exception,
                                    "Unable to unpack presence flag of optional '%s' ", rtype.c_str())

            return flag ? _binary_to_variant(ftype, ds, ctx) : json_spirit::Value();
        } else if ((s_itr = structs.find(rtype)) != structs.end()) {
            json_spirit::Object obj;
            const auto &st = s_itr->second;
            if (st.base != type_name()) {
                json_spirit::Value base = _binary_to_variant(resolve_type(st.base), ds, ctx);
                if (base.type() == json_spirit::obj_type) {
                    obj = base.get_obj();
                } else {
                    //base in array or single value
                    json_spirit::Config::add(obj, st.base, base);
                }
            }

            for (uint32_t i = 0; i < st.fields.size(); ++i) {
                // std::cout << "---------------------------------------------- " << std::endl ;
                const auto &field = st.fields[i];
                auto v = _binary_to_variant(_remove_bin_extension(field.type), ds, ctx);
                json_spirit::Config::add(obj, field.name, v);
            }
            return json_spirit::Value(std::move(obj));
        }

        WASM_THROW(unpack_exception, "Unable to unpack '%s' from stream", rtype.c_str());


        json_spirit::Value var;
        return var;
    }


    json_spirit::Value abi_serializer::binary_to_variant( const type_name &type, const bytes &binary,
                                                          microseconds max_serialization_time ) const {
        wasm::datastream<const char *> ds(binary.data(), binary.size());
        wasm::abi_traverse_context ctx(max_serialization_time);
        return _binary_to_variant(type, ds, ctx);
    }

    inline auto GetFieldVariant( const json_spirit::Value &v, field_name field, uint32_t index ) {
        if (v.type() == json_spirit::obj_type) {
            //std::cout << "GetFieldValue: field " << field << std::endl ;
            auto o = v.get_obj();
            for (json_spirit::Object::const_iterator iter = o.begin(); iter != o.end(); ++iter) {
                string name = Config_type::get_name(*iter);
                if (name == field) {
                    return Config_type::get_value(*iter);
                }
            }
        }

        WASM_TRACE("%s", field.c_str())

        WASM_THROW(invalid_type_inside_abi, "Missing %s", field.data());

        json_spirit::Value var;
        return var;

    }

    void abi_serializer::_variant_to_binary( const type_name &type, const json_spirit::Value &var,
                                             wasm::datastream<char *> &ds, wasm::abi_traverse_context &ctx ) const {
        ctx.check_deadline();
        //std::cout << "[recursion_depth:" << ctx.recursion_depth <<" type:" << type << "]"<< std::endl ;
        ctx.recursion_depth++;
        try {
            type_name rtype = resolve_type(type);

            auto s_itr = structs.end();
            auto btype = built_in_types.find(fundamental_type(rtype));
            if (btype != built_in_types.end()) {
                // std::cout  << "[built_in_types]" << btype->first << std::endl;
                btype->second.second(var, ds, is_array(rtype), is_optional(rtype));
            } else if (is_array(rtype)) {
                auto t = var.get_array();
                ds << (wasm::unsigned_int) t.size();
                for (json_spirit::Array::const_iterator iter = t.begin(); iter != t.end(); ++iter) {
                    _variant_to_binary(fundamental_type(rtype), *iter, ds, ctx);
                }
            } else if ((s_itr = structs.find(rtype)) != structs.end()) {

               // WASM_TRACE("%s", rtype.c_str())
                const auto &st = s_itr->second;
                if (var.type() == json_spirit::obj_type) {
                    if (st.base != type_name()) {
                        _variant_to_binary(resolve_type(st.base), var, ds, ctx);
                    }
                    auto &vo = var.get_obj();
                    for (uint32_t i = 0; i < st.fields.size(); ++i) {
                        // std::cout << "---------------------------------------------- " << std::endl ;
                        const auto &field = st.fields[i];
                        auto v = GetFieldVariant(vo, field.name, i);
                        //std::cout << "[GetFieldVariant]" << field.name << std::endl;
                        //WASM_TRACE("%s", field.type.c_str())
                        _variant_to_binary(_remove_bin_extension(field.type), v, ds, ctx);
                    }
                } else {
                    // WASM_TRACE("%s", json_spirit::write(var).c_str())
                    // WASM_TRACE("%s", rtype.c_str())
                    WASM_THROW(invalid_type_inside_abi, "Unknown type %s", type.c_str())
                }

            } else {
                WASM_THROW(invalid_type_inside_abi, "Unknown type %s", type.c_str());
            }
        }
        WASM_CAPTURE_AND_RETHROW("Can not convert %s to %s", type.c_str(), json_spirit::write(var).c_str())

    }

    bytes abi_serializer::_variant_to_binary( const type_name &type, const json_spirit::Value &var,
                                              wasm::abi_traverse_context &ctx ) const {
        ctx.check_deadline();

        if (!_is_type(type, ctx)) {
            bytes b;
            return b;
        }

        bytes temp(1024 * 1024);
        wasm::datastream<char *> ds(temp.data(), temp.size());
        _variant_to_binary(type, var, ds, ctx);
        temp.resize(ds.tellp());
        return temp;
    }

    bytes abi_serializer::variant_to_binary( const type_name &type, const json_spirit::Value &var,
                                             microseconds max_serialization_time ) const {
        wasm::abi_traverse_context ctx(max_serialization_time);
        return _variant_to_binary(type, var, ctx);
    }

    void abi_serializer::variant_to_binary( const type_name &type, const json_spirit::Value &var,
                                            wasm::datastream<char *> &ds, microseconds max_serialization_time ) const {
        wasm::abi_traverse_context ctx(max_serialization_time);
        _variant_to_binary(type, var, ds, ctx);
    }


    type_name abi_serializer::get_action_type( type_name action ) const {
        auto itr = actions.find(action);
        if (itr != actions.end()) return itr->second;
        return type_name();
    }

    type_name abi_serializer::get_table_type( type_name table ) const {
        auto itr = tables.find(table);
        if (itr != tables.end()) return itr->second;
        return type_name();
    }

    // optional<string> abi_serializer::get_error_message( uint64_t error_code )const {
    //    auto itr = error_messages.find( error_code );
    //    if( itr == error_messages.end() )
    //       return optional<string>();

    //    return itr->second;
    // }

    void abi_serializer::validate( wasm::abi_traverse_context &ctx ) const {

        //WASM_TRACE("%d", structs.size() )  

        for (const auto &t : typedefs) {
            try {
                vector <type_name> types_seen{t.first, t.second};
                auto itr = typedefs.find(t.second);
                while (itr != typedefs.end()) {
                    //WASM_TRACE("%s", "validate" ) 
                    ctx.check_deadline();
                    WASM_ASSERT(find(types_seen.begin(), types_seen.end(), itr->second) == types_seen.end(),
                                abi_circular_def_exception, "Circular reference in type %s",
                                itr->second.c_str());

                    types_seen.emplace_back(itr->second);
                    itr = typedefs.find(itr->second);
                }
            }
            WASM_CAPTURE_AND_RETHROW("Unknown new type %s", t.first.c_str())
        }

        //WASM_TRACE("%s", "validate" ) 
        for (const auto &t : typedefs) {
            //WASM_TRACE("%s", t.second.c_str() ) 
            try {
                WASM_ASSERT(_is_type(t.second, ctx), invalid_type_inside_abi,
                            "Unknown type %s", t.second.c_str());
            }
            WASM_CAPTURE_AND_RETHROW("Unknown type %s", t.second.c_str())
        }

       // WASM_TRACE("%s", "validate" ) 

        for (const auto &s : structs) {
            try {
                if (s.second.base != type_name()) {
                    struct_def current = s.second;
                    vector <type_name> types_seen{current.name};
                    while (current.base != type_name()) {
                        ctx.check_deadline();
                        const auto &base = get_struct(current.base); //<-- force struct to inherit from another struct

                        //std::cout << "base:" << current.base << std::endl;
                        WASM_ASSERT(find(types_seen.begin(), types_seen.end(), base.name) == types_seen.end(),
                                    abi_circular_def_exception,
                                    "Circular reference in struct %s", s.second.name.c_str());

                        types_seen.emplace_back(base.name);
                        current = base;
                    }
                }
                //WASM_TRACE("%s", "validate" ) 
                for (const auto &field : s.second.fields) {
                    try {
                        ctx.check_deadline();
                        WASM_ASSERT(_is_type(_remove_bin_extension(field.type), ctx), invalid_type_inside_abi,
                                    "Invalid type inside abi in type %s", field.type.c_str());
                    }
                    WASM_CAPTURE_AND_RETHROW("Parse error in struct %s field %s", s.first.c_str(), field.type.c_str())
                }

            }
            WASM_CAPTURE_AND_RETHROW("Parse error in struct %s", s.first.c_str())
        }

        //check struct in recursion
        auto root = make_shared<dag>(wasm::dag{"root", nullptr, vector<shared_ptr<dag>>{}, vector<shared_ptr<dag>>{}});
        root->ancestor = root;
        for (const auto &s : structs) {
            try{
                wasm::abi_traverse_context ctx2(max_serialization_time);
                check_struct_in_recursion(s.second, root, ctx2);
            }
            WASM_CAPTURE_AND_RETHROW("Circular reference in struct %s", s.first.c_str())
        }


        //WASM_TRACE("%s", "validate" ) 
        for (const auto &a : actions) {
            try {
                ctx.check_deadline();
                WASM_ASSERT(_is_type(a.second, ctx), invalid_type_inside_abi,
                            "Invalid type inside abi in action %s", a.second.c_str());
            }
            WASM_CAPTURE_AND_RETHROW("action %s error", a.first.c_str())
        }

        for (const auto &t : tables) {
            try {
                ctx.check_deadline();
                WASM_ASSERT(_is_type(t.second, ctx), invalid_type_inside_abi,
                            "Invalid type inside abi in table %s", t.second.c_str());
            }
            WASM_CAPTURE_AND_RETHROW("Table %s error", t.first.c_str())
        }
    }


    void abi_traverse_context::check_deadline() const {
        WASM_ASSERT(system_clock::now() < deadline, abi_serialization_deadline_exception,
                    "Serialization time limit %ldus exceeded", max_serialization_time_us.count());
    }


    void abi_serializer::check_struct_in_recursion(const struct_def& s, shared_ptr<dag>& parent, wasm::abi_traverse_context &ctx) const { 

        //WASM_TRACE("%s" , parent->to_string().c_str());
        auto ret = wasm::dag::add(parent, s.name, ctx);

        //s already in dag
        if(!std::get<0>(ret)) return;
        auto d = std::get<1>(ret);

        //WASM_TRACE("%s" , parent->to_string().c_str());
        ctx.check_deadline();

        vector<type_name>  fields_seen;
        for (const auto &field : s.fields) {
            ctx.check_deadline(); 
            auto f = resolve_type(fundamental_type(field.type));

            //skip same field type
            auto itr_field = std::find(fields_seen.begin(), fields_seen.end(), f); 
            if( itr_field != fields_seen.end())
                break;

            fields_seen.push_back(f);

            auto itr = structs.find(f);
            if ( itr != structs.end() ){
                check_struct_in_recursion(itr->second, d, ctx );
            }  

        }

    }

}