pragma solidity >=0.4.22 <0.6.0;

/** Taken from the OpenZeppelin github
 * @title SafeMath
 * @dev Math operations with safety checks that revert on error
 */
library SafeMath {

  /**
  * @dev Multiplies two numbers, reverts on overflow.
  */
  function mul(uint256 a, uint256 b) internal pure returns (uint256) {
    // Gas optimization: this is cheaper than requiring 'a' not being zero, but the
    // benefit is lost if 'b' is also tested.
    // See: https://github.com/OpenZeppelin/openzeppelin-solidity/pull/522
    if (a == 0) {
      return 0;
    }

    uint256 c = a * b;
    require(c / a == b);

    return c;
  }

  /**
  * @dev Integer division of two numbers truncating the quotient, reverts on division by zero.
  */
  function div(uint256 a, uint256 b) internal pure returns (uint256) {
    require(b > 0); // Solidity only automatically asserts when dividing by 0
    uint256 c = a / b;
    // assert(a == b * c + a % b); // There is no case in which this doesn't hold

    return c;
  }

  /**
  * @dev Subtracts two numbers, reverts on overflow (i.e. if subtrahend is greater than minuend).
  */
  function sub(uint256 a, uint256 b) internal pure returns (uint256) {
    require(b <= a);
    uint256 c = a - b;

    return c;
  }

  /**
  * @dev Adds two numbers, reverts on overflow.
  */
  function add(uint256 a, uint256 b) internal pure returns (uint256) {
    uint256 c = a + b;
    require(c >= a);

    return c;
  }

  /**
  * @dev Divides two numbers and returns the remainder (unsigned integer modulo),
  * reverts when dividing by zero.
  */
  function mod(uint256 a, uint256 b) internal pure returns (uint256) {
    require(b != 0);
    return a % b;
  }
}

contract WXEQERC20Token {
    
    using SafeMath for *;
    
     struct mintXEQStruct {
        address addr;
        uint amount;
        string xeqAddress;
    }
    
    struct TxStorage {
        string hash;
        uint amountMinted;
    }
    
    string public _name;
    string public _symbol;
    address public contractCreator;
    address[] public minter;
    uint256 public _totalSupply;
    uint8 public _decimals;
    mapping (address => uint256) public balanceOf;
    mapping(address => mapping (address => uint256)) allowed;
    // mintXEQStruct[] public waitingForXEQMint;
    
    event Transfer(address indexed from, address indexed to, uint256 value );
    event Approval(address indexed owner, address indexed spender, uint256 value);
    event Mint(address indexed owner, address spender, uint256 value);
    event Burn(address indexed owner, uint256 value);

    constructor() public {
        _decimals = 18;
        _name = "Wrapped Equilibria";                                 
        _symbol = "wXEQ";   
        contractCreator = msg.sender;
    }
    
    function decimals() public view returns (uint8) {
        return _decimals;
    }
    
    function name() public view returns (string memory) {
        return _name;
    }
    
    function symbol() public view returns (string memory) {
        return _symbol;
    }

    function totalSupply() public view returns (uint256) {
        return _totalSupply;
    }

    function transfer(address _to, uint256 _value) public returns (bool) {
        require(_to != address(0));
        require(balanceOf[msg.sender] >= _value);
        balanceOf[msg.sender] = balanceOf[msg.sender].sub(_value);
        balanceOf[_to] = balanceOf[_to].add(_value);
        emit Transfer(msg.sender, _to, _value);
        return true;
    }
    
    function approve(address delegate, uint256 numTokens) public returns (bool success) {
        require(delegate != address(0));
        require(balanceOf[msg.sender] >= numTokens);
        allowed[msg.sender][delegate] = allowed[msg.sender][delegate].add(numTokens);
        emit Approval(msg.sender, delegate, numTokens);
        return true;
    }
    
    function allowance(address owner, address delegate) public view returns (uint256) {
        return allowed[owner][delegate];
    }
    
    function decreaseAllowance(address delegate, uint256 _value) public returns (bool) {
        require(delegate != address(0));
        require(allowed[msg.sender][delegate] >= _value);
        allowed[msg.sender][delegate] = allowed[msg.sender][delegate].sub(_value);
        return true;
    }

    function transferFrom(address _from, address _to, uint256 numTokens) public returns (bool success) {
          require(numTokens <= balanceOf[_from]);
          require(numTokens <= allowed[_from][_to]);
          balanceOf[_from] = balanceOf[_from].sub(numTokens);
          balanceOf[_to] = balanceOf[_to].add(numTokens);
          allowed[_from][_to] = allowed[_from][_to].sub(numTokens);
          emit Transfer(_from, _to, numTokens);
          return true;
    }


    function burn(uint256 _value) public {
        require(balanceOf[msg.sender] >= _value);
        require(msg.sender != address(0x0));
        balanceOf[msg.sender] = balanceOf[msg.sender].sub(_value);            
        _totalSupply =_totalSupply.sub(_value);   
        emit Burn(msg.sender, _value);
    }
    
    // 0.5% fee is temporary - fees will eventually go to Oracle Nodes
    function devFee(uint _value) public pure returns (uint) {
        return ((_value * 50)/10000);
    }
    
    function mint(uint256 _value, address addr) public {
        require(checkMinter());
        _value = _value * 10**14;
        uint fee = devFee(_value);
        balanceOf[addr] = balanceOf[addr].add((_value - fee)); 
        balanceOf[contractCreator] = balanceOf[contractCreator].add(fee);
        _totalSupply =  _totalSupply.add(_value);  
        emit Mint(msg.sender, addr, _value);
    }
    
    function removeMinter(address addr) public returns (bool success) {
        require(msg.sender == contractCreator);
        uint origLen = minter.length;
        for (uint i = 0; i < minter.length; i++) {
            if (addr == minter[i]) {
                delete minter[i];
                assert(minter.length < origLen);
                return true;
            }
        }
    }
    
    function checkMinter() public view returns (bool success) {
        if (msg.sender == contractCreator) {
            return true;
        }
        for (uint i = 0; i < minter.length; i++) {
             if (msg.sender == minter[i] || msg.sender == contractCreator) {
                 return true;
            }
        }
        return false;
    }
    
    function addMinter(address addr) public returns (bool) {
        require(msg.sender == contractCreator);
        uint origLen = minter.length;
        minter.push(addr);
        assert(minter.length > origLen);
        return true;
    }
}
