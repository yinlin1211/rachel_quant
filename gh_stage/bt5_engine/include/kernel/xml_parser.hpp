#ifndef _XML_PARSER_H_
#define _XML_PARSER_H_
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "rapidxml_print.hpp"

namespace lquant {

    template< typename T >
    struct TypeIsString
    {
        static const bool value = false;
    };

    template<>
    struct TypeIsString< std::string >
    {
        static const bool value = true;
    };

    class XmlNode {


    private:
        XmlNode* root;
        std::vector<char> m_buffer;
        bool is_root;
        rapidxml::xml_node<>* p_root;
        rapidxml::xml_node<>* p_node;
        std::vector< std::shared_ptr<rapidxml::xml_document<> > > copied_docs;
        std::vector< std::shared_ptr<rapidxml::file<> > > copied_files;

        std::shared_ptr<rapidxml::xml_document<>> doc;
        std::shared_ptr<rapidxml::file<>> file;

    public:

        //! construct an empty doc
        XmlNode(){ is_root = false;};

        XmlNode(const XmlNode& XmlNode)
        {
            p_node = XmlNode.p_node;
            is_root = XmlNode.is_root;
            p_root = XmlNode.p_root;
            root = XmlNode.root;
            doc = XmlNode.doc;
        }

        XmlNode(rapidxml::xml_node<>* node, rapidxml::xml_node<>* rapid_root,
                std::shared_ptr<rapidxml::xml_document<>> ndoc,
                std::shared_ptr<rapidxml::file<>> nfile
        )
        {
            is_root = false;
            if (node) {
                p_node = node;
                p_root = rapid_root;
                doc = ndoc;
                file = nfile;
                root = this;
            }
            else
                std::cout << "xmlparser cannot get node" << std::endl;
            if (rapid_root==node)
                is_root = true;
        }


        XmlNode(rapidxml::xml_node<>* node, rapidxml::xml_node<>* rapid_root,
                std::shared_ptr<rapidxml::xml_document<>> ndoc,
                std::shared_ptr<rapidxml::file<>> nfile, XmlNode* rt
        )
        {
            is_root = false;
            if (node) {
                p_node = node;
                p_root = rapid_root;
                doc = ndoc;
                file = nfile;
                root = rt;
            }
            else
                std::cout << "xmlparser cannot get node" << std::endl;
            if (rapid_root==node)
                is_root = true;
        }


        //! load a root node (document) from stream
        XmlNode(std::istream &in){
            load(in);
        }

        ///load a xml file from stream
        void load(std::istream & in){

            file = std::make_shared<rapidxml::file<>>(in);

            doc = std::make_shared<rapidxml::xml_document<>>();
            doc->parse<0>(file->data());
            p_node = doc->first_node();
            p_root = p_node;
            is_root = true;
            root = this;
        }



        //! load a root node (document) from xml file
        XmlNode(std::string filename){
            load(filename);
        }
        ///load a xml file

        void load(const std::string & filename){
            std::ifstream theFile (filename);

            if (! theFile.is_open())
                std::cerr << "cannot open xml file: " << filename << std::endl;

            theFile.seekg(0, std::ios::end);
            if (0 == theFile.tellg()) {
                std::cerr << "xml file is empty" << std::endl;
            }

            file = std::make_shared<rapidxml::file<>>(filename.c_str());
            doc = std::make_shared<rapidxml::xml_document<>>();
			doc->parse<0>(file->data());
            p_node = doc->first_node();
            p_root = p_node;
            root = this;
            is_root = true;
        }

        ///close the xml file
        void unload(){
            ;
        }
        //! assignment
        XmlNode & operator= (const XmlNode & other)
        {
            if (this != &other) // protect against invalid self-assignment
            {
                // 1: allocate new memory and copy the elements
                p_node = other.p_node;
                is_root = other.is_root;
                p_root = other.p_root;
                doc = other.doc;
                root = other.root;
                file = other.file;
            }
            // by convention, always return *this
            return *this;
        }

        //! get root node
        XmlNode getRoot() const {
            return XmlNode(p_root, p_root, doc, file);
        };

        //! get parent node, return NULL if this is root
        XmlNode getParent() const {
            if (is_root)
                std::cerr << "failed to get parent of root node" << std::endl;
            else
                return XmlNode(p_node->parent(), p_root, doc, file);
        };

        //! whether the node has a child
        bool hasChild(const std::string & childid) const{
            bool is_child_exist;
            if (childid=="")
                is_child_exist = p_node->first_node();
            else
                is_child_exist = p_node->first_node(childid.c_str());

            if (is_child_exist)
                return true;
            else
                return false;
        }

        //! get the first child node
        XmlNode getChild(const std::string childid="") const{

            if (!hasChild(childid))
                std::cerr << "failed to get child:" << childid << std::endl;

            if (childid=="")
                return XmlNode(p_node->first_node(), p_root, doc, file, root);
            else
                return XmlNode(p_node->first_node(childid.c_str()), p_root, doc, file, root);
        }

        //! get all children, matching node name
        std::vector<XmlNode> getChildren(std::string nodename) const {
            std::vector<XmlNode> children;
            auto child = p_node->first_node(nodename.c_str());
            while (child) {
                children.emplace_back(child, p_root, doc, file, root);
                child = child->next_sibling(nodename.c_str());
            }
            return children;
        }

        //! get all children
        std::vector<XmlNode> getChildren() const {
            std::vector<XmlNode> children;
            auto child = p_node->first_node();
            while (child) {
                children.emplace_back(child, p_root, doc, file, root);
                child = child->next_sibling();
            }
            return children;
        }

        //! get all attributes: key is attribute name, value is attribute value
        std::unordered_map<std::string, std::string> getAllAttr() const{
            std::unordered_map<std::string, std::string> attributes;
            auto attr = p_node->first_attribute();
            while (attr) {
                attributes[attr->name()] = attr->value();
                attr = attr->next_attribute();
            }
            return attributes;
        }

        //! get all attributes: key is attribute name, value is attribute value
        std::map<std::string, std::string> getAllAttrMap() const{
            std::map<std::string, std::string> attributes;
            auto attr = p_node->first_attribute();
            while (attr) {
                attributes[attr->name()] = attr->value();
                attr = attr->next_attribute();
            }
            return attributes;
        }

        //! get a string. default is ""
        std::string getAttrOrThrow(const std::string & key) const {
            auto attr = p_node->first_attribute(key.c_str());
            if (attr)
                return std::string(attr->value());
            else
				std::cerr << "getAttrOrThrow cannot find key=" << key << std::endl;
        }

        //! get a string. default is ""
        std::string getAttrString(const std::string & key) const {
            auto attr = p_node->first_attribute(key.c_str());
            if (attr)
                return std::string(attr->value());
            else
                return "";
        }

        

        std::string print() const
        {
            std::stringstream ss;
            ss<<*p_node;
            return ss.str();
        }



        //! get a string 1
        std::string getAttrDefault(const std::string & key, const char* def_value) const{
            auto attr = p_node->first_attribute(key.c_str());
            std::string ans, ret;
            if (attr)
                ans = std::string(attr->value());
            else
                return def_value;
            return ans;
        }

        //! get a string 2
        std::string getAttrDefault(const std::string & key, const std::string def_value) const{
            return getAttrDefault(key, def_value.c_str());
        }

        //! get a string 3
        std::string getAttrDefault(const std::string & key, char* def_value) const{
            return getAttrDefault(key, const_cast<const char*>(def_value));
        }

        //! get an integer
        int getAttrDefault(const std::string & key, int def_value) const
        {
            auto attr = p_node->first_attribute(key.c_str());
            std::string ret;
            if (attr)
                ret = std::string(attr->value());
            else
                return def_value;
            return atoi(ret.c_str());
        }

        //! get a bool
        bool getAttrDefault(const std::string & key, bool def_value) const{
            auto attr = p_node->first_attribute(key.c_str());
            std::string ans, ret;
            if ( !attr ) {
                return def_value;
            }
            if ( std::string(attr->value()) == "true")
                return true;
            else if ( std::string(attr->value())=="false" )
                return false;
            else
                return def_value;
        }


        //! get a float
        float getAttrDefault(const std::string & key, float def_value) const{
            auto attr = p_node->first_attribute(key.c_str());
            std::string ret;
            if (attr)
                ret = std::string(attr->value());
            else
                return def_value;
            return atof(ret.c_str());
        }

        //! get node name
        std::string getNodeName() const{
            return p_node->name();
        }
		
		//! get node value
		std::string getNodeValue() const {
			return p_node->value();
		}
		//! get value string  eg: root.cfg1.. 
		std::string getValueDefault(const std::string & key, const char* def_value) const {
		  unsigned pos1 = 0, pos2 = 0;
			std::string sub_key;
			pos1 = key.find_first_of('.');
			XmlNode pNode = *this;
			while (pos1 != std::string::npos) {
				sub_key = key.substr(pos2, pos1 - pos2);
				if (sub_key.compare(getNodeName()) == 0) {
					pos2 = pos1 + 1;
					pos1 = key.find_first_of('.', pos2);
					continue;
				}
				if (pNode.hasChild(sub_key)){
					pNode = pNode.getChild(sub_key);
				}
				else{
					return def_value;
				}
				pos2 = pos1 + 1;
				pos1 = key.find_first_of('.', pos2);
			}
			if (pos2 != key.length()) {
				sub_key = key.substr(pos2);
				if (pNode.hasChild(sub_key)) {
					pNode = pNode.getChild(sub_key);
				}
				else {
					return def_value;
				}
			}
			return pNode.getNodeValue();
		}

		std::string getValueDefault(const std::string & key, const std::string &def_value) const {
			return getValueDefault(key, def_value.c_str());
		}

		long getValueDefault(const std::string & key, long def_value) {
			std::string value = getValueDefault(key, "");
			if (value.empty()) {
				return def_value;
			}
			else {
				return atol(value.c_str());
			}
		}

		int getValueDefault(const std::string & key, int def_value) {
			std::string value = getValueDefault(key, "");
			if (value.empty()) {
				return def_value;
			}
			else {
				return atoi(value.c_str());
			}
		}
		double getValueDefault(const std::string & key, double def_value) {
			std::string value = getValueDefault(key, "");
			if (value.empty()) {
				return def_value;
			}
			else {
				return atof(value.c_str());
			}
		}

        //! is root node
        bool isRoot() const {
            return is_root;
        }

        //! append a child node to the current node. return appended child
        XmlNode appendChild(const std::string & nodename)
        {
            char *node_name = doc->allocate_string(nodename.c_str());
            auto node = doc->allocate_node(rapidxml::node_element, node_name);
            p_node->append_node(node);
            return XmlNode(node, p_root, doc, file, root);
        }

        XmlNode appendChild(XmlNode & new_node)
        {
            auto node = doc->clone_node( new_node.p_node );
            p_node->append_node(node);
            copied_docs.push_back(new_node.doc);
            copied_files.push_back(new_node.file);
            return XmlNode(node, p_root, doc, file, root);
        }


        XmlNode appendChild(XmlNode & new_node, XmlNode & rt)
        {
            auto node = doc->clone_node( new_node.p_node );
            p_node->append_node(node);
            rt.copied_docs.push_back(new_node.doc);
            rt.copied_files.push_back(new_node.file);
            return XmlNode(node, p_root, doc, file, &rt);
        }

/*
        template<typename T>
        T appendAttr(const std::string & key, const T value) const{
            //char* rapidAttributeName = doc.allocate_string(key.c_str());
            auto attr = p_node->first_attribute(key.c_str());
            std::string ret;
            T  a = {0};
            if (attr)
                ret = std::string(attr->value());
            else
                return def_value;
            std::istringstream ss(ret);
            ss >> a;
            return a;
        }
        */

        //! append attribute to current node

        void appendAttr(const std::string & name, const char * value)
        {
            auto attr = p_node->first_attribute(name.c_str());
            char *new_value = doc->allocate_string(value);
            if (attr)
            {
                attr->value(new_value);
            }
            else
            {
                char *new_name = doc->allocate_string(name.c_str());
                auto attr = doc->allocate_attribute(new_name,new_value);
                p_node->append_attribute(attr);
            }
        }

        void appendAttr(const std::string & name, const std::string  value){
            return appendAttr(name, value.c_str());
        }
        void appendAttr(const std::string & name, float value){
            return appendAttr(name, std::to_string( (long double)value));
        }

        void appendAttr(const std::string & name, int value){
            return appendAttr(name, std::to_string( (long long)value));
        }

        void appendAttr(const std::string & name, bool value){
            if (value)
                appendAttr(name, "true");
            else
                appendAttr(name, "false");
        }


    };




}
#endif //
