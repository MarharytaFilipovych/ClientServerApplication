import java.io.*;
import java.net.*;
import java.util.Scanner;

public class Main {
    public static void main(String[] args) throws IOException {
        Socket socket = new Socket("localhost", 12345);
        Client client = new Client(socket);
        Scanner scanner = new Scanner(System.in);
        client.sendMessage("Hello from client!");
        client.outputServerResponse();
        String userInput = scanner.nextLine();
        while(true){
            if(userInput.trim().equalsIgnoreCase("EXIT")){
                System.out.println("Client decided to terminate the connection");
                break;
            }
            if(Request.isInvalidRequest(userInput.trim()))System.out.println("Undefined request");
            else client.processRequest(userInput.trim());
            userInput = scanner.nextLine();
        }
        socket.close();
        scanner.close();
    }
}