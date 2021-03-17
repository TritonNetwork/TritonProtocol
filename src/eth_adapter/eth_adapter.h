
#include <string>
#include <vector>
class EthAdapter {

    std::string myAddress;
    std::string infuraURL;
    
    public:
        EthAdapter();
        void mintWXEQ(std::string account, std::string amount);
};
