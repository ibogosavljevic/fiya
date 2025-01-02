
#pragma once

#include <vector>
#include <ostream>
#include <cassert>
#include <cstring>
#include <functional>
#include <unordered_map>

#include "fiya-string-db.h"

namespace fiya {

/** Class counter_inteface_t exposes counters
 *  to components that are not interested about labels
 */
template <typename MeasureType>
class counter_interface_t {
public:
    /** Reading counter value */
    virtual const MeasureType& cnt() const = 0;

    /** Modifying counter value */
    virtual MeasureType& cnt() = 0;

    /** Returns true if the counter_interface_t is
     *  performing internal operations 
     */
    virtual bool recorder_internal_running() const = 0;
};

/** Class scoping_interface_t exposes scoping
  * to components not interested about counters
  */
template <typename LabelType>
class scoping_interface_t {
public:
    /** Begins a scope with a given label */
    virtual void begin_scope(const LabelType& label) = 0;
    
    /** Ends the scope for the current label. */
    virtual void end_scope() = 0;

    /** Ends the scope for the current label. The
     *  parameter @label is used to assert corectness,
     *  but it is not mandatory
     */
    virtual void end_scope(const LabelType& label) = 0;

    /** Returns true if the scoping_interface_t is
     *  performing internal operations 
     */
    virtual bool recorder_internal_running() const = 0;
};

/** Helper for working with labels. Some
  * type of labels (notably const char * labels)
  * require different handling than the one provided
  * here.
  */
template<typename LabelType>
class label_helper {
public:
    /** Returns true if two labels are equal */
    virtual bool equal(const LabelType& l1, const LabelType& l2) const {
        return l1 == l2;
    }

    /** Converts a label from external to internal representation */
    LabelType save(const LabelType& l) {
        return l;
    }

    /** Converts a label from internal to external representation */
    LabelType restore(const LabelType& l) const {
        return l;
    }
};

template<>
class label_helper<const char*> {
    static_assert(sizeof(size_t) == sizeof(const char *),
    "The const char * implementation uses a hack that only "
    "works if sizeof(size_t) == sizeof(const char *), which "
    "should be true everywhere");
public:
    /** Returns true if two labels are equal 
     * the label @l1 has the internal representation,
     * the label @l2 has the external representation.
     */
    bool equal(const char* const & l1, const char* const & l2) const {
        size_t idx { reinterpret_cast<size_t>(l1) };
        return strcmp(m_string_db.get(idx), l2) == 0;
    }

    /** Converts a label from external to internal representation.
     *  Saves the string to internal database and in the code references
     *  the string from there.
     */
    const char* save(const char* const & l) {
        size_t r { m_string_db.push_back(l) };
        return reinterpret_cast<const char* const>(r);
    }

    /** Converts a label from internal to external representation.
     *  Finds the string with index @l in the string database.
     */
    const char* restore(const char* const & l) const {
        size_t idx { reinterpret_cast<const size_t>(l) };
        return m_string_db.get(idx);
    }

private:
    /** String database */
    string_db_t m_string_db;
};

/** Forward declaration needed for report_t class */
template<typename LabelType, typename MeasureType>
class recorder_t;

/** The recorder report */
template<typename LabelType, typename MeasureType>
class report_t {
public:
    /** @brief For each label, this struct holds the measured values:
     *   @param self value is the value recorded while the label was active
     *          excluding the value recorded by all the other nested labels.
     *   @param total time is the value recorded while the label was active
     *          including all the nested labels that were started while this
     *          label was active.
    */
    struct report_entry_t {
        MeasureType self;
        MeasureType total;
    };

    std::unordered_map<LabelType, report_entry_t> report;
private:
    report_t(const label_helper<LabelType> label_helper) : 
        m_helper(label_helper) {}
    /** @brief Label helper is used to store the label databased 
     *         if label database is used.
     */
    label_helper<LabelType> m_helper;

    friend class recorder_t<LabelType, MeasureType>;
};

/** The recorder class */
template<typename LabelType, typename MeasureType>
class recorder_t: public counter_interface_t<MeasureType>, public scoping_interface_t<LabelType> {
public:
    /** Constructor
     *
     *    @param default_value Measure value used to initialize newly constructed nodes
     *    @param root_label    Name of the root label, constructed when the recorder is constructed
     *    @param root_value    Measure value for the root label
     */
    recorder_t(MeasureType default_value, const LabelType & root_label, MeasureType root_value) :
        m_recorder_internal_running(true),
        m_default_value(default_value),
        m_root(new measure_node_t(m_label_helper.save(root_label), root_value, nullptr)),
        m_current_node(m_root)
    {
        m_recorder_internal_running = false;
    }

    /** Destructor */
    virtual ~recorder_t() {
        m_recorder_internal_running = true;
        delete_node(m_root);
    }

    /**
     * @brief Begins a new scope with a given label.
     *
     * @note begin_scope and end_scope need to be matched.
     * 
     * @param label 
     */
    void begin_scope(const LabelType& label) override {
        m_recorder_internal_running = true;
        measure_node_t* node { nullptr };
        
        std::vector<measure_node_t*>& children = m_current_node->m_children;
        for(size_t i { 0U }; i < children.size(); i++) {
            if (m_label_helper.equal(children[i]->m_label, label)) {
                node = children[i];
                break;
            }
        }

        if (node == nullptr) {
            // Node not found, generate a new node
            children.emplace_back(new measure_node_t(m_label_helper.save(label), m_default_value, m_current_node));
            node = children.back();
        }

        m_current_node = node;
        m_recorder_internal_running = false;
    }

    /** Ends a scope */
    void end_scope() override {
        m_recorder_internal_running = true;
        assert(m_current_node->m_parent != nullptr);
        m_current_node = m_current_node->m_parent;
        m_recorder_internal_running = false;
    }

    /** Ends a scope. This overload also checks the scope is 
      * properly closed by comparing @label with the current scope's
      * label.
      */
    void end_scope(const LabelType& label) override {
        m_recorder_internal_running = true;
        assert(m_current_node->m_parent != nullptr);
        assert(m_label_helper.equal(m_current_node->m_label, label));
        m_current_node = m_current_node->m_parent;
        m_recorder_internal_running = false;
    }

    /** Returns the counter for the current scope */
    const MeasureType& cnt() const override {
        return m_current_node->m_value;
    }

    /** Returns the counter for the current scope */
    MeasureType& cnt() override {
        return m_current_node->m_value;
    }

    /** Returns true if the recording APIs are running.
     *
     *  @note This is useful in some applications in order,
              because recorder function can also trigger
              external events, in which case we would
              like to avoid
        */
    bool recorder_internal_running() const override {
        return m_recorder_internal_running;
    }

    /** @brief Boilerplate to check if a type has operator<< */
    template <typename T, typename = void>
    struct has_ostream_operator : std::false_type {};

    /** @brief Boilerplate to check if a type has operator<< */
    template <typename T>
    struct has_ostream_operator<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>> : std::true_type {};

    /** @brief Converts the internal representation to a text you can
     *         use to feed the flamegraph drawer.
     *
     *  @param os Output stream where to write the result
     *  @param l_out Function used to output the label type to output stream
     *  @param m_out Function used to output the measure type to output stream
     */
    void to_collapsed_stacks(
        std::ostream& os,
        const std::function<void(std::ostream& os, const LabelType& l)> & l_out,
        const std::function<void(std::ostream& os, const MeasureType& m)> & m_out)
    {
        m_recorder_internal_running = true;
        to_collapsed_stack(m_root, os, l_out, m_out);
        m_recorder_internal_running = false;
    }

    /** @brief Converts the internal representation to a text you can
     *         use to feed the flamegraph drawer.
     *
     *  @param os Output stream where to write the result
     *  @param m_out Function used to output the measure type to output stream
     */
    template <typename T = LabelType>
    std::enable_if_t<has_ostream_operator<T>::value>
    to_collapsed_stacks(
        std::ostream& os,
        const std::function<void(std::ostream& os, const MeasureType& m)> & m_out
    ) {
        to_collapsed_stack(m_root, os, m_out, value_out<LabelType>);
    }

    /** @brief Converts the internal representation to a text you can
     *         use to feed the flamegraph drawer.
     *
     *  @param os Output stream where to write the result
     *  @param m_out Function used to output the measure type to output stream
     */
    template <typename T = MeasureType>
    std::enable_if_t<has_ostream_operator<T>::value> 
    to_collapsed_stacks(
        std::ostream& os,
        const std::function<void(std::ostream& os, const LabelType& l)> & l_out)
    {
        to_collapsed_stack(m_root, os, value_out<MeasureType>, l_out);
    }

    /** @brief Converts the internal representation to a text you can
     *         use to feed the flamegraph drawer.
     *
     *  @param os Output stream where to write the result
     */
    template <typename T1 = LabelType, typename T2 = MeasureType>
    std::enable_if_t<has_ostream_operator<T1>::value && has_ostream_operator<T2>::value> 
    to_collapsed_stacks(std::ostream& os) {
        to_collapsed_stack(m_root, os, value_out<MeasureType>, value_out<LabelType>);
    }


    /** @brief Boilerplate to check if a type has operator+ */
    template <typename T, typename = void>
    struct has_plus_operator : std::false_type {};

    /** @brief Boilerplate to check if a type has operator+ */
    template <typename T>
    struct has_plus_operator<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>> : std::true_type {};

    using my_report_type = report_t<LabelType, MeasureType>;

    /** @brief Converts the measured values to per label report.
     *  @param accumulate_op Pointer to the accumulate operations (needed to generate `total` value).
     */
    template<typename Operation>
    my_report_type to_report(const Operation& accumulate_op) {
        m_recorder_internal_running = true;
        my_report_type result(m_label_helper);
        to_report(m_root, result, accumulate_op);
        m_recorder_internal_running = false;
        return result;
    }

    /** @brief Converts the measured values to per label report.
      */
    template <typename T = MeasureType>
    std::enable_if_t<has_plus_operator<T>::value, my_report_type> 
    to_report() {
        m_recorder_internal_running = true;
        my_report_type result(m_label_helper);
        auto accumulate_op = std::plus<T>();
        to_report(m_root, result, accumulate_op);
        m_recorder_internal_running = false;
        return result;
    }
private:
    /** Flag set while the recorder is running, see  @recorder_internal_running. */
    bool m_recorder_internal_running;
    /** Label helper, used for more efficient storage of strings. */
    label_helper<LabelType> m_label_helper;
    /** Default value used to initialize a new measure value first time a new scope is opened. */
    MeasureType const m_default_value;

    /** measure_node */
    struct measure_node_t {
        /** Node label */
        LabelType const m_label;
        /** Node value */
        MeasureType m_value;
        /** Parent node */
        measure_node_t* m_parent;
        /** Children nodes */
        std::vector<measure_node_t*> m_children; 

        /** Constructor */
        measure_node_t(const LabelType& label, MeasureType value, measure_node_t* parent):
            m_label(label),
            m_value(value),
            m_parent(parent)
        {
        }
    };

    /** Scope root */
    measure_node_t* m_root;
    /** Scope current note */
    measure_node_t* m_current_node;
    
    /** @brief Deletes the node and all of it children. */
    void delete_node(measure_node_t* node) {
        std::vector<measure_node_t*>& children = node->m_children;
        for (size_t i { 0U }; i < children.size(); ++i) {
            delete_node(children[i]);
        }

        delete node;
    }

    /** @brief Outputs the value using operator<<. */
    template <typename T>
    static std::enable_if_t<has_ostream_operator<T>::value> 
    value_out(std::ostream &os, const T& value) {
        os << value;
    }

    /** @brief Prints the stack (chain of nested labels) recursively. */
    void print_stack(
        measure_node_t* node,
        std::ostream& os,
        const std::function<void(std::ostream& os, const LabelType& l)> & l_out
    ) {
        if (node->m_parent) {
            print_stack(node->m_parent, os, l_out);
            os << ";";
        }

        l_out(os, m_label_helper.restore(node->m_label));
    }

    /** @brief Prints one line of collapsed stack */
    void to_collapsed_stack(
        measure_node_t* node,
        std::ostream& os,
        const std::function<void(std::ostream& os, const LabelType& l)> & l_out,
        const std::function<void(std::ostream& os, const MeasureType& m)> & m_out
    ) {
        print_stack(node, os, l_out);
        os << " ";
        m_out(os, node->m_value);
         
        os << "\n";

        std::vector<measure_node_t*>& children = node->m_children;
        for (size_t i { 0U }; i < children.size(); ++i) {
            to_collapsed_stack(children[i], os, l_out, m_out);
        }
    }

    /**  @brief Generates report for the current node in the tree. */
    template<typename Operation>
    MeasureType to_report(measure_node_t* node, my_report_type& report, const Operation& op) {
        MeasureType total = node->m_value;
        const std::vector<measure_node_t*>& children = node->m_children;
        for (size_t i = { 0U }; i < children.size(); ++i) {
            total = op(total, to_report(children[i], report, op));
        }

        LabelType label = report.m_helper.restore(node->m_label);

        auto it = report.report.find(label);
        if (it != report.report.end()) {
            it->second.self = op(it->second.self, node->m_value);
            it->second.total = op(it->second.total, total);
        } else {
            typename my_report_type::report_entry_t entry;
            entry.self = node->m_value;
            entry.total = total;
            report.report.insert(std::pair<LabelType, typename my_report_type::report_entry_t>{ label, entry });
        }

        return total;
    }
};

}